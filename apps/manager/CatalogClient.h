#pragma once
#include <QCryptographicHash>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QTemporaryFile>
#include <QTimer>
#include <QVariant>
#include <QtQml/qqmlregistration.h>
#include <memory>

class CatalogClient : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(CatalogClient)
    QML_UNCREATABLE("Owned by Manager")
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool demo READ demo NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString errorCode READ errorCode NOTIFY changed)
    Q_PROPERTY(int page READ page NOTIFY changed)
    Q_PROPERTY(int pages READ pages NOTIFY changed)
    Q_PROPERTY(int total READ total NOTIFY changed)
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString downloadId READ downloadId NOTIFY changed)
    Q_PROPERTY(qint64 received READ received NOTIFY changed)
    Q_PROPERTY(qint64 expected READ expected NOTIFY changed)
  public:
    explicit CatalogClient(QObject* parent = nullptr, QNetworkAccessManager* network = nullptr,
                           QString cacheDirectory = {});
    ~CatalogClient() override;
    void configure(const QVariantMap& settings);
    static QString configurationError(const QVariantMap& settings);
    Q_INVOKABLE void useConfigured();
    QVariantList items() const { return items_; }
    QVariantList categories() const { return categories_; }
    bool loading() const { return loading_; }
    bool demo() const { return base_.scheme() == "mock" || serverDemo_; }
    QString error() const { return error_; }
    QString errorCode() const;
    int page() const { return page_; }
    int pages() const { return pages_; }
    int total() const { return total_; }
    QString phase() const { return phase_; }
    QString downloadId() const { return downloadId_; }
    qint64 received() const { return received_; }
    qint64 expected() const { return expected_; }
    Q_INVOKABLE void browse(const QString& query = "", const QString& category = "",
                            const QString& sort = "new", int page = 1);
    Q_INVOKABLE void useDemo();
    Q_INVOKABLE void download(const QString& id);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retry();
    Q_INVOKABLE void openLicense(const QString& id);
    void installed();
    void installing() {
        phase_ = "installing";
        emit changed();
    }
    void importFailed(const QString& message);
    void requestInstall();
    bool allowedUrl(const QUrl& url) const;
  signals:
    void changed();
    void packageReady(QString path, QString sceneId, QString sha256, qint64 size, QString source);

  private:
    void setBase(const QUrl& base);
    void fetchCategories();
    void previews(int generation);
    void downloadTo(const QUrl& url, int redirects);
    void beginDownload(const QVariantMap& entry);
    void fail(const QString& message);
    QNetworkReply* get(const QUrl& url);
    void validatePage(const QByteArray& bytes);
    void trimCache();
    QNetworkAccessManager* network_;
    QUrl base_;
    QStringList origins_;
    bool local_ = false, loading_ = false, serverDemo_ = false;
    QVariantMap configuration_, downloadEntry_;
    QVariantList items_, categories_;
    QString cache_, error_, phase_ = "idle", downloadId_, readyPath_;
    QString query_, category_, sort_ = "new";
    int page_ = 1, pages_ = 1, total_ = 0, generation_ = 0, previewActive_ = 0, previewNext_ = 0,
        redirects_ = 0;
    qint64 received_ = 0, expected_ = 0;
    QPointer<QNetworkReply> pageReply_, downloadReply_, categoryReply_;
    QSet<QNetworkReply*> previewReplies_;
    std::unique_ptr<QTemporaryFile> temporary_;
    std::unique_ptr<QCryptographicHash> hash_;
    QFutureWatcher<QString> validation_;
    QTimer deadline_;
};
