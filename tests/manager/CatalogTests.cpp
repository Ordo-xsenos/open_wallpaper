#include "CatalogClient.h"
#include "CatalogNetwork.h"
#include "TestServer.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class FaultNetwork : public CatalogNetwork {
  public:
    enum Mode { Normal, Corrupt, Oversize, Truncate, UnsafeUrl, Downgrade, WrongId } mode = Normal;

  protected:
    QNetworkReply* createRequest(Operation operation, const QNetworkRequest& request,
                                 QIODevice* outgoing) override {
        if (request.url().path() == "/api/v1/wallpapers" &&
            (mode == UnsafeUrl || mode == WrongId)) {
            QFile file(":/catalog/catalog.json");
            if (!file.open(QIODevice::ReadOnly))
                qFatal("Missing catalog fixture");
            auto entries =
                QJsonDocument::fromJson(file.readAll()).object().value("items").toArray();
            auto item = entries.first().toObject();
            for (const auto& key : {"packageUrl", "previewUrl", "licenseProofUrl"})
                item[key] = "mock://catalog/" + item.value(key).toString();
            if (mode == UnsafeUrl)
                item["packageUrl"] = "file:///private/package.awscene";
            else
                item["id"] = "11111111-1111-4111-8111-111111111111";
            return new MemoryReply(request,
                                   QJsonDocument(QJsonObject{{"schemaVersion", 1},
                                                             {"page", 1},
                                                             {"limit", 24},
                                                             {"total", 1},
                                                             {"pages", 1},
                                                             {"demo", true},
                                                             {"items", QJsonArray{item}}})
                                       .toJson(),
                                   200, this);
        }
        if (request.url().path().endsWith("package.awscene")) {
            if (mode == Downgrade) {
                class RedirectReply : public MemoryReply {
                  public:
                    RedirectReply(const QNetworkRequest& r, QObject* p)
                        : MemoryReply(r, {}, 302, p) {
                        setAttribute(QNetworkRequest::RedirectionTargetAttribute,
                                     QUrl("http://untrusted.invalid/package.awscene"));
                    }
                };
                return new RedirectReply(request, this);
            }
            if (mode == Corrupt || mode == Oversize || mode == Truncate) {
                QFile file(":/catalog" + request.url().path());
                if (!file.open(QIODevice::ReadOnly))
                    qFatal("Missing catalog fixture");
                auto bytes = file.readAll();
                if (mode == Corrupt)
                    bytes[bytes.size() - 1] ^= 1;
                if (mode == Oversize)
                    bytes.append('x');
                if (mode == Truncate)
                    bytes.chop(1);
                return new MemoryReply(request, bytes, 200, this);
            }
        }
        return CatalogNetwork::createRequest(operation, request, outgoing);
    }
};

class CatalogTests : public QObject {
    Q_OBJECT
  private slots:
    void examplesSearchFilterAndVerifiedDownload() {
        QTemporaryDir cache;
        CatalogClient client(nullptr, nullptr, cache.path());
        QSignalSpy ready(&client, &CatalogClient::packageReady);
        client.useDemo();
        QTRY_COMPARE_WITH_TIMEOUT(client.items().size(), 6, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(
            !client.items().first().toMap().value("previewLocal").toUrl().isEmpty(), 5000);
        client.browse("parallax");
        QTRY_COMPARE_WITH_TIMEOUT(client.items().size(), 1, 5000);
        const auto item = client.items().first().toMap();
        client.download(item.value("id").toString());
        QTRY_VERIFY_WITH_TIMEOUT(client.phase() == "ready" || client.phase() == "error", 5000);
        QVERIFY2(client.phase() == "ready", qPrintable(client.error()));
        QCOMPARE(ready.size(), 1);
        QVERIFY(QFile::exists(ready.at(0).at(0).toString()));
        TestServer core("", false);
        core.service->library().remove(item.value("id").toString().toStdString());
        const auto args = ready.first();
        const auto response = aetherwall::parseJson(core.service->handle(aetherwall::Json{
            {"version", 1},
            {"requestId", "download-test"},
            {"command", "importDownloadedScene"},
            {"params",
             {{"path", args[0].toString().toStdString()},
              {"sceneId", args[1].toString().toStdString()},
              {"sha256", args[2].toString().toStdString()},
              {"size", args[3].toLongLong()},
              {"source", args[4].toString().toStdString()}}}}.dump()));
        QVERIFY(response.at("ok").get<bool>());
        QCOMPARE(core.service->library().downloadHistory().size(), 1u);
        QVERIFY(!core.applied); // Importing never changes wallpaper assignment.
    }
    void verifiedPackageSurvivesClientLifetimeAndCanBeDownloadedAgain() {
        QTemporaryDir cache;
        QString path;
        QString digest;
        qint64 size = 0;
        for (int attempt = 0; attempt < 2; ++attempt) {
            {
                CatalogClient client(nullptr, nullptr, cache.path());
                QSignalSpy ready(&client, &CatalogClient::packageReady);
                client.useDemo();
                QTRY_COMPARE_WITH_TIMEOUT(client.items().size(), 6, 5000);
                client.download(client.items().first().toMap().value("id").toString());
                QTRY_VERIFY_WITH_TIMEOUT(client.phase() == "ready" || client.phase() == "error",
                                         5000);
                QVERIFY2(client.phase() == "ready", qPrintable(client.error()));
                QCOMPARE(ready.size(), 1);
                const auto args = ready.first();
                if (attempt == 0) {
                    path = args[0].toString();
                    digest = args[2].toString();
                    size = args[3].toLongLong();
                } else {
                    QCOMPARE(args[0].toString(), path);
                }
            }
            // The committed package must remain readable after the client and
            // its temporary file have been destroyed, including on Windows.
            QFile package(path);
            QVERIFY2(package.open(QIODevice::ReadOnly), qPrintable(package.errorString()));
            QCOMPARE(package.size(), size);
            QCOMPARE(QString::fromLatin1(
                         QCryptographicHash::hash(package.readAll(), QCryptographicHash::Sha256)
                             .toHex()),
                     digest);
            QCOMPARE(
                QDir(cache.path()).entryList({".download-*"}, QDir::Files | QDir::Hidden).size(),
                0);
        }
    }
    void corruptTruncatedOversizedAndRedirectedPackagesNeverInstall() {
        for (auto mode : {FaultNetwork::Corrupt, FaultNetwork::Oversize, FaultNetwork::Truncate,
                          FaultNetwork::Downgrade}) {
            QTemporaryDir cache;
            FaultNetwork network;
            network.mode = mode;
            CatalogClient client(nullptr, &network, cache.path());
            QSignalSpy ready(&client, &CatalogClient::packageReady);
            client.useDemo();
            QTRY_COMPARE_WITH_TIMEOUT(client.items().size(), 6, 5000);
            client.download(client.items().first().toMap().value("id").toString());
            QTRY_COMPARE_WITH_TIMEOUT(client.phase(), QString("error"), 5000);
            QCOMPARE(ready.size(), 0);
            QVERIFY(!client.errorCode().isEmpty());
            QCOMPARE(QDir(cache.path()).entryList(QStringList{"*.awscene"}, QDir::Files).size(), 0);
        }
    }
    void wrongIdentityAndUnsafeMetadataAreRejected() {
        for (auto mode : {FaultNetwork::WrongId, FaultNetwork::UnsafeUrl}) {
            QTemporaryDir cache;
            FaultNetwork network;
            network.mode = mode;
            CatalogClient client(nullptr, &network, cache.path());
            QSignalSpy ready(&client, &CatalogClient::packageReady);
            client.useDemo();
            QTRY_VERIFY_WITH_TIMEOUT(!client.loading(), 5000);
            if (mode == FaultNetwork::UnsafeUrl) {
                QVERIFY(client.items().empty());
                QVERIFY(!client.error().isEmpty());
            } else {
                QCOMPARE(client.items().size(), 1);
                client.download(client.items().first().toMap().value("id").toString());
                QTRY_COMPARE_WITH_TIMEOUT(client.phase(), QString("error"), 5000);
            }
            QCOMPARE(ready.size(), 0);
        }
    }
    void cancellationIsCleanAndRetrySucceeds() {
        QTemporaryDir cache;
        CatalogClient client(nullptr, nullptr, cache.path());
        client.useDemo();
        QTRY_COMPARE_WITH_TIMEOUT(client.items().size(), 6, 5000);
        client.download(client.items().first().toMap().value("id").toString());
        client.cancel();
        QCOMPARE(client.phase(), QString("cancelled"));
        client.browse("no such wallpaper");
        QTRY_VERIFY_WITH_TIMEOUT(!client.loading(), 5000);
        QVERIFY(client.items().empty());
        client.retry();
        QTRY_VERIFY_WITH_TIMEOUT(client.phase() == "ready" || client.phase() == "error", 5000);
        QVERIFY2(client.phase() == "ready", qPrintable(client.error()));
    }
    void urlPolicyRequiresHttpsOrExplicitLoopbackAndAllowlistedAssets() {
        QTemporaryDir cache;
        CatalogClient client(nullptr, nullptr, cache.path());
        QVariantMap settings{{"catalogBaseUrl", "https://catalog.example"},
                             {"catalogAssetOrigins", QVariantList{"https://cdn.example"}},
                             {"allowLocalCatalog", false}};
        QVERIFY(CatalogClient::configurationError(settings).isEmpty());
        client.configure(settings);
        QVERIFY(client.allowedUrl(QUrl("https://cdn.example/file")));
        QVERIFY(!client.allowedUrl(QUrl("https://unknown.example/file")));
        QVERIFY(!client.allowedUrl(QUrl("http://catalog.example/file")));
        QVERIFY(!client.allowedUrl(QUrl("file:///tmp/file")));
        settings["catalogBaseUrl"] = "http://127.0.0.1:8765";
        QVERIFY(!CatalogClient::configurationError(settings).isEmpty());
        settings["allowLocalCatalog"] = true;
        QVERIFY(CatalogClient::configurationError(settings).isEmpty());
        client.configure(settings);
        QVERIFY(client.allowedUrl(QUrl("http://127.0.0.1:8765/file")));
        QVERIFY(!client.allowedUrl(QUrl("http://127.0.0.1:9999/file")));
        settings["catalogBaseUrl"] = "http://example.com";
        QVERIFY(!CatalogClient::configurationError(settings).isEmpty());
    }
};
QTEST_GUILESS_MAIN(CatalogTests)
#include "CatalogTests.moc"
