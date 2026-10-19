#include "DebugLog.h"
#include "aetherwall_version.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <cstdio>

namespace {
struct State {
    QMutex mutex;
    QFile file;
    QString directory;
    bool capped = false;
};
State& state() {
    // Keep the handler and its storage alive through Qt's shutdown diagnostics.
    static auto* value = new State;
    return *value;
}
void append(const QString& level, QString message) {
    auto& log = state();
    QMutexLocker lock(&log.mutex);
    if (!log.file.isOpen() || log.capped)
        return;
    message.replace('\r', "\\r").replace('\n', "\\n");
    auto line =
        (QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs) + " [" + level + "] [thread=" +
         QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()), 16) + "] " +
         message.left(32768) + '\n')
            .toUtf8();
    if (log.file.size() + line.size() > 16 * 1024 * 1024) {
        line = "[WARN] Manager log limit reached; further entries suppressed.\n";
        log.capped = true;
    }
    log.file.write(line);
    log.file.flush();
}
void handler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    const char* level = "DEBUG";
    switch (type) {
    case QtInfoMsg:
        level = "INFO";
        break;
    case QtWarningMsg:
        level = "WARN";
        break;
    case QtCriticalMsg:
        level = "ERROR";
        break;
    case QtFatalMsg:
        level = "FATAL";
        break;
    default:
        break;
    }
    append(QString::fromLatin1(level), QString::fromUtf8(context.category) + ": " + message);
}
} // namespace
namespace DebugLog {
bool initialize(const QStringList& arguments) {
    if (!arguments.contains("--debug"))
        return true;
    auto& log = state();
    if (log.file.isOpen())
        return true;
    const auto index = arguments.indexOf("--debug-dir");
    if (index >= 0 && (index + 1 >= arguments.size() || arguments[index + 1].startsWith("--"))) {
        std::fprintf(stderr, "--debug-dir requires a directory\n");
        return false;
    }
    auto base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#ifdef Q_OS_WIN
    base = qEnvironmentVariable("LOCALAPPDATA");
#endif
    log.directory = index >= 0
                        ? arguments[index + 1]
                        : base + "/AetherWall/diagnostics/" +
                              QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz") +
                              "_" + QString::number(QCoreApplication::applicationPid());
    log.directory = QDir(log.directory).absolutePath();
    log.file.setFileName(log.directory + "/manager_" +
                         QString::number(QCoreApplication::applicationPid()) + ".log");
    if (!QDir().mkpath(log.directory) || !log.file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        std::fprintf(stderr, "Cannot open Manager debug log: %s\n",
                     qPrintable(log.file.fileName()));
        return false;
    }
    // Install before QGuiApplication so plugin, platform and QML startup failures survive.
    qInstallMessageHandler(handler);
    qputenv("QT_DEBUG_PLUGINS", "1");
    QLoggingCategory::setFilterRules("qt.scenegraph.general=true\nqt.qpa.gl=true");
    write("AetherWall " AETHERWALL_VERSION " Manager debug session; pid=" +
          QString::number(QCoreApplication::applicationPid()) +
          "; OS=" + QSysInfo::prettyProductName() + "; kernel=" + QSysInfo::kernelVersion());
    return true;
}
bool enabled() { return state().file.isOpen(); }
QString directory() { return state().directory; }
void write(const QString& message) {
    if (enabled())
        append("TRACE", message);
}
} // namespace DebugLog
