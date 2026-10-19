#include "DebugLog.h"
#include "ManagerController.h"
#include "aetherwall_version.h"
#ifdef _WIN32
#include "system/WindowsSupport.h"
#include <shellapi.h>
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
    QStringList startupArguments;
#ifdef _WIN32
    int nativeCount = 0;
    auto** nativeArguments = CommandLineToArgvW(GetCommandLineW(), &nativeCount);
    if (!nativeArguments)
        return 2;
    for (int index = 0; index < nativeCount; ++index)
        startupArguments.append(QString::fromWCharArray(nativeArguments[index]));
    LocalFree(nativeArguments);
#else
    for (int index = 0; index < argc; ++index)
        startupArguments.append(QString::fromLocal8Bit(argv[index]));
#endif
    if (!DebugLog::initialize(startupArguments))
        return 2;
    DebugLog::write("Entering Manager startup before QGuiApplication");
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
    DebugLog::write("QGuiApplication initialized; executable=" + app.applicationFilePath());
    const auto args = app.arguments();
    const bool capture = args.contains("--capture");
    QLocalServer instance;
    if (!capture && !args.contains("--allow-multiple")) {
        QLocalSocket existing;
        existing.connectToServer("AetherWall-Manager-v1");
        if (existing.waitForConnected(150)) {
            DebugLog::write("Another Manager instance is already running; activating it. Exit "
                            "AetherWall completely before a debug session.");
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
        transport = [&](const std::string& bytes) {
            if (args.contains("--test-disconnect") &&
                aetherwall::parseJson(bytes).at("command") == "getMetrics")
                throw std::runtime_error(
                    "TEST: Core transport disconnected after loading displays.");
            auto response = aetherwall::parseJson(harness->service->handle(bytes));
            if (args.contains("--test-render-failure") &&
                aetherwall::parseJson(bytes).at("command") == "getStatus") {
                response["result"]["renderState"] = "failed";
                response["result"]["renderError"] =
                    "TEST: Desktop attachment unavailable. Library remains connected.";
            }
            return response.dump();
        };
    }
#endif
    ManagerController controller(args.contains("--core-window"), nullptr, std::move(transport));
    if (args.contains("--mock-catalog"))
        controller.catalog()->useDemo();
    if (capture && args.contains("--capture-catalog-offline"))
        controller.catalog()->configure({{"catalogBaseUrl", "http://127.0.0.1:1"},
                                         {"catalogAssetOrigins", QVariantList()},
                                         {"allowLocalCatalog", true}});
    if (!capture && !args.contains("--no-auto-core"))
        controller.enableAutoStart();
    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"controller", QVariant::fromValue(&controller)}});
    DebugLog::write("Loading Manager QML module");
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
        if (args.contains("--capture-delete-dialog"))
            QTimer::singleShot(1100, &app, [window] {
                if (auto* dialog = window->findChild<QObject*>("deleteDialog"))
                    QMetaObject::invokeMethod(dialog, "open");
            });
        const auto index = args.indexOf("--capture");
        if (index + 1 >= args.size())
            return 2;
        QTimer::singleShot(1500, &app, [&, path = args.at(index + 1)] {
            auto* captureWindow = window;
            if (window && window->property("page").toInt() >= 3)
                if (auto* settings = window->findChild<QQuickWindow*>("settingsWindow"))
                    captureWindow = settings;
            app.exit(captureWindow && captureWindow->grabWindow().save(path) ? 0 : 1);
        });
    }
    DebugLog::write("Manager window ready; entering event loop");
    const auto result = app.exec();
    DebugLog::write("Manager event loop exited; code=" + QString::number(result));
    return result;
}
