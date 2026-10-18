#pragma once
#include "control/ControlService.h"
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QTimer>

// Real storage/protocol over a test Unix/Qt socket. It never owns a D3D renderer.
class TestServer {
  public:
    QTemporaryDir directory;
    QLocalServer server;
    bool paused = false, stopped = false, applied = false;
    std::unique_ptr<aetherwall::ControlService> service;
    explicit TestServer(const QString& name, bool listen = true) {
        using aetherwall::Json;
        aetherwall::RuntimeControl runtime;
        runtime.status = [this] {
            return Json{{"running", true},
                        {"mode", "headless-test"},
                        {"dataDirectory", directory.path().toStdString()},
                        {"logDirectory", directory.path().toStdString()}};
        };
        runtime.monitors = [this] {
            return Json::array({{{"id", "test-display"},
                                 {"alias", "TEST DISPLAY · no GPU"},
                                 {"primary", true},
                                 {"width", 1920},
                                 {"height", 1080},
                                 {"left", 0},
                                 {"top", 0},
                                 {"refreshHz", 60},
                                 {"dpi", 96},
                                 {"manualPause", paused},
                                 {"pauseReasons", paused ? 1 : 0}}});
        };
        runtime.metrics = [] { return Json::array(); };
        runtime.updateSettings = [](const auto&) {};
        runtime.prepareApply = [this](const auto&, const auto&) -> std::function<void()> {
            return [this] { applied = true; };
        };
        runtime.pause = [this](bool value, const auto&) { paused = value; };
        runtime.stop = [this] { stopped = true; };
        runtime.reload = [] {};
        runtime.active = [](const auto&) { return false; };
        service = std::make_unique<aetherwall::ControlService>(
            aetherwall::textPath(directory.path().toStdString()), std::move(runtime));
        service->seed(AETHERWALL_BUNDLED_DIR);
        QObject::connect(&server, &QLocalServer::newConnection, &server, [this] {
            while (auto* socket = server.nextPendingConnection()) {
                auto buffer = std::make_shared<QByteArray>();
                QObject::connect(socket, &QLocalSocket::readyRead, socket, [this, socket, buffer] {
                    *buffer += socket->readAll();
                    if (buffer->size() < 4)
                        return;
                    try {
                        const auto length = aetherwall::frameLength(std::span(
                            reinterpret_cast<const std::uint8_t*>(buffer->constData()), 4));
                        if (buffer->size() < static_cast<qsizetype>(length) + 4)
                            return;
                        const auto response =
                            service->handle(std::string_view(buffer->constData() + 4, length));
                        const auto frame = aetherwall::frameMessage(response);
                        socket->write(reinterpret_cast<const char*>(frame.data()),
                                      static_cast<qint64>(frame.size()));
                        socket->disconnectFromServer();
                    } catch (...) {
                        socket->abort();
                    }
                });
                QObject::connect(socket, &QLocalSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
        if (!listen)
            return;
        server.setSocketOptions(QLocalServer::UserAccessOption);
        if (!server.listen(name))
            throw std::runtime_error(server.errorString().toStdString());
    }
};
