#include "ManagerController.h"
#include "CoreProcess.h"
#include "DebugLog.h"
#include "storage/SettingsStore.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QSysInfo>
#include <QUrl>
#include <algorithm>
#ifdef _WIN32
#include "ipc/NamedPipeServer.h"
#endif

namespace {
using aetherwall::Json;
QVariant variant(const Json& json) {
    const auto bytes = json.dump();
    return QJsonDocument::fromJson(QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())))
        .toVariant();
}
Json jsonVariant(const QVariant& value) {
    return aetherwall::parseJson(
        QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact).toStdString());
}
std::string exchange(const std::string& request) {
#ifdef _WIN32
    return aetherwall::pipeExchange(request);
#else
    // Development-only Unix local socket. The production engine remains Windows-only.
    QLocalSocket socket;
    socket.connectToServer("AetherWall-Core-v1");
    if (!socket.waitForConnected(500))
        throw std::runtime_error("Core is not available");
    const auto bytes = aetherwall::frameMessage(request);
    socket.write(reinterpret_cast<const char*>(bytes.data()), static_cast<qint64>(bytes.size()));
    if (socket.bytesToWrite() && !socket.waitForBytesWritten(1000))
        throw std::runtime_error("Core write timeout");
    QByteArray received;
    auto read = [&](qsizetype length) {
        while (received.size() < length) {
            if (!socket.bytesAvailable() && !socket.waitForReadyRead(2000))
                throw std::runtime_error("Core response timeout");
            received += socket.read(length - received.size());
        }
    };
    read(4);
    const auto size = aetherwall::frameLength(
        std::span(reinterpret_cast<const std::uint8_t*>(received.constData()), 4));
    read(static_cast<qsizetype>(size) + 4);
    return received.mid(4).toStdString();
#endif
}
} // namespace
ManagerController::ManagerController(bool diagnostic, QObject* parent, Exchange transport)
    : ManagerController(diagnostic, parent, std::move(transport), LaunchHooks{}) {}
ManagerController::ManagerController(bool diagnostic, QObject* parent, Exchange transport,
                                     LaunchHooks launcher)
    : QObject(parent), launcher_(std::move(launcher)), diagnostic_(diagnostic),
      exchange_(transport ? std::move(transport) : exchange) {
    if (!launcher_.start) {
        auto process = std::make_shared<CoreProcess>();
        launcher_.start = [this, process] {
            const auto executable = QCoreApplication::applicationDirPath() + "/AetherWall.Core.exe";
            if (!QFileInfo::exists(executable))
                return language() == "ru"
                           ? QStringLiteral("Файл AetherWall.Core.exe не найден. Переустановите "
                                            "AetherWall.\n") +
                                 executable
                           : QStringLiteral(
                                 "AetherWall.Core.exe is missing. Reinstall AetherWall.\n") +
                                 executable;
            QDir().mkpath(logDirectory());
            DebugLog::write("Launching Core; executable=" + executable +
                            "; logs=" + logDirectory());
            return process->start(executable, diagnostic_, logDirectory(), DebugLog::enabled());
        };
        launcher_.exitCode = [process] { return process->exitCode(); };
    }
    catalog_ = new CatalogClient(this);
    connect(catalog_, &CatalogClient::packageReady, this,
            [this](const QString& path, const QString& id, const QString& sha, qint64 size,
                   const QString& source) {
                if (!connected_ || pendingMutation_)
                    return;
                catalog_->installing();
                mutate("importDownloadedScene", {{"path", path.toStdString()},
                                                 {"sceneId", id.toStdString()},
                                                 {"sha256", sha.toStdString()},
                                                 {"size", size},
                                                 {"source", source.toStdString()}});
            });
    settings_ = variant(aetherwall::defaultSettings()).toMap();
    catalog_->configure(settings_);
    catalog_->useDemo();
    timer_.setInterval(40);
    connect(&timer_, &QTimer::timeout, this, &ManagerController::tick);
    timer_.start();
    refreshTimer_.setInterval(3000);
    connect(&refreshTimer_, &QTimer::timeout, this, &ManagerController::refresh);
    refreshTimer_.start();
    refresh();
}
ManagerController::~ManagerController() {
    if (hasFuture_)
        future_.wait();
}
void ManagerController::refresh() {
    if (hasFuture_ || !tasks_.empty() || pendingMutation_)
        return;
    for (const auto* command :
         {"getStatus", "getSettings", "listInstalledScenes", "getMonitors", "getMetrics"})
        enqueue({command, Json::object(), false});
}
void ManagerController::enqueue(Task task) { tasks_.push_back(std::move(task)); }
void ManagerController::tick() {
    checkStartup();
    if (hasFuture_) {
        if (future_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        hasFuture_ = false;
        try {
            const auto response = aetherwall::parseJson(future_.get());
            aetherwall::onlyFields(response, {"version", "requestId", "ok", "result", "error"});
            if (response.at("version") != 1 || response.at("requestId") != activeId_ ||
                !response.at("ok").is_boolean())
                throw std::runtime_error("Invalid Core response");
            DebugLog::write("IPC reply id=" + QString::fromStdString(activeId_) +
                            "; command=" + QString::fromStdString(active_.command) +
                            "; ok=" + QString::number(response.at("ok").get<bool>()));
            connected_ = true;
            launchState_ = "connected";
            launchError_.clear();
            refreshTimer_.setInterval(3000);
            if (!response.at("ok").get<bool>()) {
                error_ =
                    QString::fromStdString(response.at("error").at("message").get<std::string>());
                if (active_.command == "importDownloadedScene")
                    catalog_->importFailed(error_);
                if (active_.mutation) {
                    pendingMutation_ = false;
                    tasks_.clear();
                }
            } else
                accept(active_, response.at("result"));
        } catch (const std::exception& error) {
            DebugLog::write("IPC failure id=" + QString::fromStdString(activeId_) +
                            "; command=" + QString::fromStdString(active_.command) +
                            "; error=" + QString::fromUtf8(error.what()));
            if (DebugLog::enabled() && launcher_.exitCode) {
                if (const auto code = launcher_.exitCode())
                    DebugLog::write(QStringLiteral("Observed Core exit code=0x%1")
                                        .arg(*code, 8, 16, QLatin1Char('0')));
            }
            if (active_.mutation)
                error_ = QString::fromUtf8(error.what());
            if (active_.command == "importDownloadedScene")
                catalog_->importFailed(error_);
            const bool lostConnection = connected_;
            connected_ = false;
            if (launchState_ != "starting" && launchState_ != "error") {
                if (lostConnection) {
                    launchState_ = "error";
                    launchError_ = QString::fromUtf8(error.what());
                } else if (autoStart_ && !launchAttempted_) {
                    startCore();
                } else {
                    launchState_ = "error";
                    launchError_ = QString::fromUtf8(error.what());
                }
            }
            pendingMutation_ = false;
            tasks_.clear();
        }
        if (active_.mutation) {
            pendingMutation_ = false;
            refresh();
        }
        emit changed();
    }
    if (!hasFuture_ && !tasks_.empty()) {
        active_ = std::move(tasks_.front());
        tasks_.pop_front();
        activeId_ = std::to_string(++nextId_);
        DebugLog::write("IPC send id=" + QString::fromStdString(activeId_) +
                        "; command=" + QString::fromStdString(active_.command));
        const auto request = Json{
            {"version", 1},
            {"requestId", activeId_},
            {"command", active_.command},
            {"params",
             active_.params}}.dump();
        future_ = std::async(std::launch::async,
                             [request, transport = exchange_] { return transport(request); });
        hasFuture_ = true;
    }
}
void ManagerController::accept(const Task& task, const Json& value) {
    if (task.command == "getStatus")
        status_ = variant(value).toMap();
    else if (task.command == "getSettings" || task.command == "updateSettings") {
        settings_ = variant(value).toMap();
        catalog_->configure(settings_);
        if (settings_.value("catalogBaseUrl").toString().isEmpty() && !catalog_->demo())
            catalog_->useDemo();
    } else if (task.command == "listInstalledScenes") {
        scenes_ = variant(value).toList();
        for (auto& entry : scenes_) {
            auto scene = entry.toMap();
            scene["previewUrl"] = QUrl::fromLocalFile(scene.value("previewPath").toString());
            scene["folderUrl"] = QUrl::fromLocalFile(scene.value("path").toString());
            entry = scene;
        }
    } else if (task.command == "getMonitors") {
        monitors_ = variant(value.at("monitors")).toList();
        const auto assignments = variant(value.at("assignments")).toMap();
        for (auto& entry : monitors_) {
            auto m = entry.toMap();
            m["sceneId"] = assignments.value(m.value("id").toString());
            entry = m;
        }
    } else if (task.command == "getMetrics")
        metrics_ = variant(value).toList();
    else if (task.command == "importDownloadedScene")
        catalog_->installed();
    else if (task.command == "stop")
        QCoreApplication::quit();
}
void ManagerController::mutate(const std::string& command, Json params) {
    if (!connected_ || pendingMutation_)
        return;
    pendingMutation_ = true;
    error_.clear();
    tasks_.clear();
    enqueue({command, std::move(params), true});
    emit changed();
}
void ManagerController::enableAutoStart() {
    autoStart_ = true;
    // The first status request must finish before deciding whether to launch.
    if (!hasFuture_ && tasks_.empty() && !connected_ && !launchAttempted_)
        startCore();
}
void ManagerController::startCore() {
    if (connected_ || launchState_ == "starting")
        return;
    launchAttempted_ = true;
    DebugLog::write("Requesting Core process start");
    launchError_ = launcher_.start();
    DebugLog::write(launchError_.isEmpty() ? "Core process created; waiting for IPC"
                                           : "Core start failed: " + launchError_);
    if (!launchError_.isEmpty())
        launchError_.prepend(language() == "ru" ? QStringLiteral("Не удалось запустить фон. ")
                                                : QStringLiteral("Wallpaper could not start. "));
    launchState_ = launchError_.isEmpty() ? "starting" : "error";
    if (launchError_.isEmpty()) {
        startupClock_.start();
        refreshTimer_.setInterval(500);
    }
    emit changed();
}
void ManagerController::checkStartup() {
    if (launchState_ != "starting")
        return;
    if (const auto code = launcher_.exitCode ? launcher_.exitCode() : std::nullopt) {
        launchError_ =
            (language() == "ru"
                 ? QStringLiteral("Фоновый процесс завершился до подключения (0x%1). Откройте "
                                  "журнал или скопируйте диагностику.")
                 : QStringLiteral(
                       "Core exited before connecting (0x%1). Open logs or copy diagnostics."))
                .arg(*code, 8, 16, QLatin1Char('0'));
    } else if (startupClock_.elapsed() >= launcher_.timeoutMs) {
        launchError_ =
            language() == "ru"
                ? QStringLiteral(
                      "Core не ответил за 15 секунд. Откройте журнал или скопируйте диагностику.")
                : QStringLiteral(
                      "Core did not respond within 15 seconds. Open logs or copy diagnostics.");
    } else {
        return;
    }
    DebugLog::write("Core startup failed: " + launchError_);
    launchState_ = "error";
    refreshTimer_.setInterval(3000);
    emit changed();
}
QString ManagerController::logDirectory() const {
    if (DebugLog::enabled())
        return DebugLog::directory();
    const auto reported = status_.value("logDirectory").toString();
    if (!reported.isEmpty())
        return reported;
#ifdef _WIN32
    return QDir::fromNativeSeparators(qEnvironmentVariable("LOCALAPPDATA")) + "/AetherWall/logs";
#else
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
           "/AetherWall/logs";
#endif
}
QString ManagerController::diagnosticText() const {
    QString result = QStringLiteral("AetherWall %1\nOS: %2 (%3)\nLaunch: %4\nLaunch error: "
                                    "%5\nRender: %6\nRender error: %7\nLogs: %8\n")
                         .arg(QCoreApplication::applicationVersion(), QSysInfo::prettyProductName(),
                              QSysInfo::kernelVersion(), launchState_, launchError_,
                              status_.value("renderState").toString(),
                              status_.value("renderError").toString(), logDirectory());
    if (DebugLog::enabled())
        result += QStringLiteral("Manager debug: enabled; Core debug: %1\nCore reported logs: %2\n")
                      .arg(status_.value("debugLogging").toBool() ? "enabled" : "disabled/unknown",
                           status_.value("logDirectory").toString());
    const auto files = QDir(logDirectory()).entryInfoList({"m0_*.log"}, QDir::Files, QDir::Time);
    if (!files.empty()) {
        QFile file(files.first().absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            file.seek(std::max<qint64>(0, file.size() - 16384));
            result += "\n" + QString::fromUtf8(file.read(16384));
        }
    }
    return result;
}
void ManagerController::copyDiagnostics() {
    if (qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
        QGuiApplication::clipboard()->setText(diagnosticText());
}
void ManagerController::importScene(const QUrl& path) {
    if (path.isLocalFile())
        mutate("importScene", {{"path", path.toLocalFile().toStdString()}});
}
void ManagerController::apply(const QString& monitor, const QString& scene) {
    mutate("applyScene", {{"monitorId", monitor.toStdString()}, {"sceneId", scene.toStdString()}});
}
void ManagerController::favorite(const QString& scene, bool enabled) {
    mutate("setFavorite", {{"sceneId", scene.toStdString()}, {"favorite", enabled}});
}
void ManagerController::removeScene(const QString& scene) {
    mutate("deleteScene", {{"sceneId", scene.toStdString()}});
}
void ManagerController::pause(bool enabled, const QString& monitor) {
    mutate(enabled ? "pause" : "resume", {{"monitorId", monitor.toStdString()}});
}
void ManagerController::updateSettings(const QVariantMap& settings) {
    const auto configurationError = CatalogClient::configurationError(settings);
    if (!configurationError.isEmpty()) {
        error_ = settings_.value("language").toString() == "ru"
                     ? QStringLiteral(
                           "Адрес каталога отклонён. Укажите HTTPS-адрес и проверьте разрешённые "
                           "адреса файлов; HTTP разрешён только для локальной разработки.")
                     : configurationError;
        emit changed();
        return;
    }
    mutate("updateSettings", {{"settings", jsonVariant(settings)}});
}
void ManagerController::resetSettings() {
    mutate("updateSettings", {{"settings", aetherwall::defaultSettings()}});
}
void ManagerController::reloadDisplays() { mutate("reloadDisplayTopology", Json::object()); }
void ManagerController::exitCompletely() { mutate("stop", Json::object()); }
void ManagerController::openFolder(const QUrl& folder) {
    if (folder.isLocalFile())
        QDesktopServices::openUrl(folder);
}
void ManagerController::openLogs() {
    QDir().mkpath(logDirectory());
    openFolder(QUrl::fromLocalFile(logDirectory()));
}
void ManagerController::dismissError() {
    error_.clear();
    emit changed();
}
QVariantList ManagerController::filterScenes(QVariantList values, const QString& query,
                                             bool favorites, int sort) const {
    values.erase(std::remove_if(values.begin(), values.end(),
                                [&](const QVariant& entry) {
                                    const auto scene = entry.toMap();
                                    return (favorites && !scene.value("favorite").toBool()) ||
                                           !(scene.value("title").toString() + " " +
                                             scene.value("author").toString())
                                                .contains(query, Qt::CaseInsensitive);
                                }),
                 values.end());
    std::stable_sort(values.begin(), values.end(), [sort](const QVariant& a, const QVariant& b) {
        const auto x = a.toMap(), y = b.toMap();
        if (sort == 1 && x.value("favorite") != y.value("favorite"))
            return x.value("favorite").toBool();
        return QString::localeAwareCompare(x.value("title").toString(),
                                           y.value("title").toString()) < 0;
    });
    return values;
}

void ManagerController::installCatalogPackage() {
    if (connected_ && !pendingMutation_)
        catalog_->requestInstall();
}
