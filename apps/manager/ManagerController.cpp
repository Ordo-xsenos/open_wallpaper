#include "ManagerController.h"
#include "storage/SettingsStore.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QProcess>
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
    : QObject(parent), diagnostic_(diagnostic),
      exchange_(transport ? std::move(transport) : exchange) {
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
            connected_ = true;
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
            if (active_.mutation)
                error_ = QString::fromUtf8(error.what());
            if (active_.command == "importDownloadedScene")
                catalog_->importFailed(error_);
            connected_ = false;
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
void ManagerController::startCore() {
#ifdef _WIN32
    const auto executable = QCoreApplication::applicationDirPath() + "/AetherWall.Core.exe";
    if (!QFileInfo::exists(executable)) {
        error_ = language() == "ru"
                     ? "Core не найден рядом с Manager. Соберите и разверните оба приложения."
                     : "Core is missing beside Manager. Build and deploy both applications.";
        emit changed();
        return;
    }
    QStringList arguments{"--service"};
    if (diagnostic_)
        arguments << "--mode" << "window" << "--all-monitors";
    if (!QProcess::startDetached(executable, arguments))
        error_ = "Cannot start Core. Check its diagnostic log.";
#else
    error_ = language() == "ru"
                 ? "Движок работает только в Windows 11. Здесь доступна проверка интерфейса."
                 : "The engine requires Windows 11. This build verifies the interface only.";
#endif
    emit changed();
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
    openFolder(QUrl::fromLocalFile(status_.value("logDirectory").toString()));
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
