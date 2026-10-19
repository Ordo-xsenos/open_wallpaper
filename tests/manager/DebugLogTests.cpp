#include "DebugLog.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <thread>
#include <vector>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    const auto arguments = app.arguments();
    if (arguments.contains("--child")) {
        if (!DebugLog::initialize(arguments))
            return 2;
        qWarning("escaped\nsecond\rline");
        std::vector<std::thread> writers;
        for (int thread = 0; thread < 4; ++thread)
            writers.emplace_back([thread] {
                for (int line = 0; line < 100; ++line)
                    DebugLog::write(QString("worker=%1 line=%2").arg(thread).arg(line));
            });
        for (auto& writer : writers)
            writer.join();
        if (arguments.contains("--limit")) {
            for (int line = 0; line < 600; ++line)
                DebugLog::write(QString(32768, 'x'));
            DebugLog::write("SHOULD_BE_SUPPRESSED");
        }
        return 0;
    }
    QTemporaryDir directory;
    if (!directory.isValid())
        return 1;
    auto run = [&](const QStringList& extra, const QString& path) {
        QProcess child;
        child.start(app.applicationFilePath(),
                    QStringList{"--child", "--debug", "--debug-dir", path} + extra);
        if (!child.waitForFinished(30000) || child.exitStatus() != QProcess::NormalExit ||
            child.exitCode() != 0)
            return QByteArray{};
        const auto files = QDir(path).entryList({"manager_*.log"}, QDir::Files);
        if (files.size() != 1)
            return QByteArray{};
        QFile file(path + '/' + files.first());
        if (!file.open(QIODevice::ReadOnly))
            return QByteArray{};
        return file.readAll();
    };
    // Includes spaces/non-ASCII paths, concurrent writes and capture of Qt messages.
    const auto output = run({}, directory.path() + QString::fromUtf8("/debug сеанс"));
    if (!output.contains("escaped\\nsecond\\rline") || output.count("worker=") != 400 ||
        output.contains("\nsecond"))
        return 1;
    const auto capped = run({"--limit"}, directory.path() + "/bounded");
    if (capped.size() > 16 * 1024 * 1024 + 100 || !capped.contains("log limit reached") ||
        capped.contains("SHOULD_BE_SUPPRESSED"))
        return 1;
    QProcess invalid;
    invalid.start(app.applicationFilePath(), {"--child", "--debug", "--debug-dir"});
    if (!invalid.waitForFinished(30000) || invalid.exitCode() != 2)
        return 1;
    return 0;
}
