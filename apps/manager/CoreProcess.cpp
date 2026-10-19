#include "CoreProcess.h"
#include <QDir>
#ifdef _WIN32
#include "system/WindowsSupport.h"
#endif

CoreProcess::~CoreProcess() {
#ifdef _WIN32
    if (process_)
        CloseHandle(process_);
#endif
}
QString CoreProcess::start(const QString& executable, bool diagnostic, const QString& logDirectory,
                           bool debug) {
#ifdef _WIN32
    if (process_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT)
        return QStringLiteral("Core is already starting or running. Retry the connection.");
    if (process_) {
        CloseHandle(process_);
        process_ = nullptr;
    }
    if (executable.contains('"') || logDirectory.contains('"'))
        return QStringLiteral("A quote is not allowed in an executable or log path.");
    // Arguments are internal switches and filesystem paths, never shell commands.
    // Windows paths cannot contain a quote; a trailing slash must not escape it.
    auto quote = [](QString value) {
        value.replace('/', '\\');
        while (value.size() > 3 && value.endsWith('\\'))
            value.chop(1);
        if (value.endsWith('\\'))
            value += '\\';
        return QStringLiteral("\"") + value + QStringLiteral("\"");
    };
    QString command = quote(executable) + " --service --log-dir " + quote(logDirectory);
    if (debug)
        command += " --debug";
    if (diagnostic)
        command += " --mode window --all-monitors";
    auto buffer = command.toStdWString();
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION child{};
    const auto directory =
        QDir::toNativeSeparators(executable.left(executable.lastIndexOf('/'))).toStdWString();
    const auto path = QDir::toNativeSeparators(executable).toStdWString();
    if (!CreateProcessW(path.c_str(), buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                        nullptr, directory.c_str(), &startup, &child)) {
        const auto code = GetLastError();
        wchar_t message[1024]{};
        FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0,
                       message, 1024, nullptr);
        return QStringLiteral("CreateProcessW: 0x%1 — %2")
            .arg(code, 8, 16, QLatin1Char('0'))
            .arg(QString::fromWCharArray(message).trimmed());
    }
    CloseHandle(child.hThread);
    process_ = child.hProcess;
    return {};
#else
    Q_UNUSED(executable)
    Q_UNUSED(debug)
    Q_UNUSED(diagnostic)
    Q_UNUSED(logDirectory)
    return QStringLiteral("Core requires Windows 11 24H2 or later.");
#endif
}
std::optional<quint32> CoreProcess::exitCode() const {
#ifdef _WIN32
    if (process_ && WaitForSingleObject(process_, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        if (GetExitCodeProcess(process_, &code))
            return code;
    }
#endif
    return std::nullopt;
}
