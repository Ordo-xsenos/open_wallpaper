#include "TestServer.h"
#include <QCoreApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    try {
        TestServer server("AetherWall-Core-v1");
        std::cout
            << "HEADLESS TEST HARNESS: storage/protocol only; no Windows engine or GPU rendering.\n"
            << std::flush;
        QTimer stop;
        QObject::connect(&stop, &QTimer::timeout, &app, [&] {
            if (server.stopped)
                app.quit();
        });
        stop.start(100);
        return app.exec();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
