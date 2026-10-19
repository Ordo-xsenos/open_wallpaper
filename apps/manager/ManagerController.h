#pragma once
#include "CatalogClient.h"
#include "ipc/Protocol.h"
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <QtQml/qqmlregistration.h>
#include <deque>
#include <future>
#include <optional>

class ManagerController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(ManagerController)
    QML_UNCREATABLE("Created by the Manager executable")
    Q_PROPERTY(CatalogClient* catalog READ catalog CONSTANT)
    Q_PROPERTY(QString launchState READ launchState NOTIFY changed)
    Q_PROPERTY(QString launchError READ launchError NOTIFY changed)
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QVariantList scenes READ scenes NOTIFY changed)
    Q_PROPERTY(QVariantList monitors READ monitors NOTIFY changed)
    Q_PROPERTY(QVariantList metrics READ metrics NOTIFY changed)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
  public:
    using Exchange = std::function<std::string(const std::string&)>;
    struct LaunchHooks {
        std::function<QString()> start;
        std::function<std::optional<quint32>()> exitCode;
        int timeoutMs = 15000;
    };
    explicit ManagerController(bool diagnostic, QObject* parent = nullptr, Exchange exchange = {});
    ManagerController(bool diagnostic, QObject* parent, Exchange exchange, LaunchHooks launcher);
    ~ManagerController() override;
    CatalogClient* catalog() const { return catalog_; }
    Q_INVOKABLE void installCatalogPackage();
    bool connected() const { return connected_; }
    bool busy() const { return pendingMutation_; }
    QString error() const { return error_; }
    QString language() const { return settings_.value("language", "ru").toString(); }
    QVariantList scenes() const { return scenes_; }
    QVariantList monitors() const { return monitors_; }
    QVariantList metrics() const { return metrics_; }
    QVariantMap settings() const { return settings_; }
    QVariantMap status() const { return status_; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void startCore();
    Q_INVOKABLE void enableAutoStart();
    Q_INVOKABLE QString diagnosticText() const;
    Q_INVOKABLE void copyDiagnostics();
    QString launchState() const { return launchState_; }
    QString launchError() const { return launchError_; }
    Q_INVOKABLE void importScene(const QUrl& path);
    Q_INVOKABLE void apply(const QString& monitor, const QString& scene);
    Q_INVOKABLE void favorite(const QString& scene, bool enabled);
    Q_INVOKABLE void removeScene(const QString& scene);
    Q_INVOKABLE void pause(bool enabled, const QString& monitor = "*");
    Q_INVOKABLE void updateSettings(const QVariantMap& settings);
    Q_INVOKABLE void resetSettings();
    Q_INVOKABLE void reloadDisplays();
    Q_INVOKABLE void exitCompletely();
    Q_INVOKABLE void openFolder(const QUrl& folder);
    Q_INVOKABLE void openLogs();
    Q_INVOKABLE void dismissError();
    Q_INVOKABLE QVariantList filterScenes(QVariantList values, const QString& query, bool favorites,
                                          int sort) const;
  signals:
    void changed();

  private:
    struct Task {
        std::string command;
        aetherwall::Json params;
        bool mutation = false;
    };
    void enqueue(Task task);
    void tick();
    void accept(const Task& task, const aetherwall::Json& value);
    void mutate(const std::string& command, aetherwall::Json params);
    void checkStartup();
    QString logDirectory() const;
    LaunchHooks launcher_;
    QElapsedTimer startupClock_;
    QString launchState_ = "checking", launchError_;
    bool autoStart_ = false, launchAttempted_ = false;
    bool diagnostic_ = false, connected_ = false, pendingMutation_ = false, hasFuture_ = false;
    QString error_;
    QVariantList scenes_, monitors_, metrics_;
    QVariantMap settings_, status_;
    CatalogClient* catalog_ = nullptr;
    Exchange exchange_;
    QTimer timer_, refreshTimer_;
    std::future<std::string> future_;
    std::deque<Task> tasks_;
    Task active_;
    std::uint64_t nextId_ = 0;
    std::string activeId_;
};
