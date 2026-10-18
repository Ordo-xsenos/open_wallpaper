#include "ManagerController.h"
#include "aetherwall_version.h"
#ifdef _WIN32
#include "system/WindowsSupport.h"
#endif
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#ifdef AETHERWALL_CAPTURE_HARNESS
#include "TestServer.h"
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
    aetherwall::UniqueHandle runningMutex(
        CreateMutexW(nullptr, FALSE, L"Local\\AetherWall.Manager.Running"));
    if (!runningMutex) {
        qCritical("Cannot create Manager running marker");
        return 1;
    }
#endif
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationVersion(QStringLiteral(AETHERWALL_VERSION));
    QGuiApplication::setApplicationName("AetherWall");
    QGuiApplication::setOrganizationName("AetherWall");
    app.setWindowIcon(QIcon("qrc:/qt/qml/AetherWall/Manager/assets/aetherwall.png"));
    QQuickStyle::setStyle("Fusion");
    QImageReader::setAllocationLimit(32);
    const auto args = app.arguments();
    const bool capture = args.contains("--capture");
    QLocalServer instance;
    if (!capture && !args.contains("--allow-multiple")) {
        QLocalSocket existing;
        existing.connectToServer("AetherWall-Manager-v1");
        if (existing.waitForConnected(150)) {
            existing.write("activate");
            existing.waitForBytesWritten(150);
            return 0;
        }
        instance.setSocketOptions(QLocalServer::UserAccessOption);
        // Never remove an arbitrary endpoint left by another live process.
        instance.listen("AetherWall-Manager-v1");
    }
    ManagerController::Exchange transport;
#ifdef AETHERWALL_CAPTURE_HARNESS
    std::unique_ptr<TestServer> harness;
    if (args.contains("--test-library")) {
        harness = std::make_unique<TestServer>(QString(), false);
        if (args.contains("--test-english")) {
            auto settings = aetherwall::defaultSettings();
            settings["language"] = "en";
            harness->service->handle(aetherwall::Json{{"version", 1},
                                                      {"requestId", "capture-setup"},
                                                      {"command", "updateSettings"},
                                                      {"params", {{"settings", settings}}}}
                                         .dump());
        }
        transport = [&](const std::string& bytes) { return harness->service->handle(bytes); };
    }
#endif
    ManagerController controller(args.contains("--core-window"), nullptr, std::move(transport));
    if (args.contains("--mock-catalog"))
        controller.catalog()->useDemo();
    if (capture && args.contains("--capture-catalog-offline"))
        controller.catalog()->configure({{"catalogBaseUrl", "http://127.0.0.1:1"},
                                         {"catalogAssetOrigins", QVariantList()},
                                         {"allowLocalCatalog", true}});
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}});
    engine.loadFromModule("AetherWall.Manager", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QObject::connect(&instance, &QLocalServer::newConnection, &app, [&] {
        while (auto* socket = instance.nextPendingConnection()) {
            socket->close();
            socket->deleteLater();
        }
        if (window) {
            window->showNormal();
            window->raise();
            window->requestActivate();
        }
    });
    if (args.contains("--dump-layout")) {
        QTimer::singleShot(1000, &app, [window] {
            for (auto* item : window->findChildren<QQuickItem*>())
                if (!item->objectName().isEmpty())
                    qInfo() << item->objectName() << item->x() << item->y() << item->width()
                            << item->height();
        });
    }
    if (capture) {
        const int pageIndex = args.indexOf("--capture-page"),
                  sizeIndex = args.indexOf("--capture-size");
        if (pageIndex >= 0 && pageIndex + 1 < args.size())
            window->setProperty("page", args.at(pageIndex + 1).toInt());
        if (sizeIndex >= 0 && sizeIndex + 1 < args.size()) {
            const auto parts = args.at(sizeIndex + 1).split('x');
            if (parts.size() == 2)
                window->resize(parts.at(0).toInt(), parts.at(1).toInt());
        }
        if (args.contains("--capture-select-first"))
            QTimer::singleShot(1000, &app, [&] {
                if (window->property("page").toInt() == 1 &&
                    !controller.catalog()->items().empty()) {
                    if (auto* page = window->findChild<QObject*>("discoverPage"))
                        page->setProperty("selected", controller.catalog()->items().first());
                } else if (!controller.scenes().empty())
                    window->setProperty("selected", controller.scenes().first());
            });
        const auto index = args.indexOf("--capture");
        if (index + 1 >= args.size())
            return 2;
        QTimer::singleShot(1500, &app, [&, path = args.at(index + 1)] {
            app.exit(window && window->grabWindow().save(path) ? 0 : 1);
        });
    }
    return app.exec();
}
