#include "ManagerController.h"
#include "TestServer.h"
#include <QTest>

class ManagerTests : public QObject {
    Q_OBJECT
  private slots:
    void liveLibrarySettingsAndEngineIndependence() {
        TestServer core("", false);
        {
            ManagerController manager(false, nullptr, [&](const std::string& bytes) {
                return core.service->handle(bytes);
            });
            QTRY_COMPARE_WITH_TIMEOUT(manager.scenes().size(), 6, 5000);
            QTRY_COMPARE_WITH_TIMEOUT(manager.monitors().size(), 1, 5000);
            QVERIFY(manager.connected());
            const auto id = manager.scenes().first().toMap().value("id").toString();
            manager.apply("*", id);
            QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 5000);
            QVERIFY(core.applied);
            auto settings = manager.settings();
            settings["language"] = "en";
            settings["fps"] = 15;
            manager.updateSettings(settings);
            QTRY_COMPARE_WITH_TIMEOUT(manager.language(), QString("en"), 5000);
            QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 5000);
            manager.pause(true, "*");
            QTRY_VERIFY_WITH_TIMEOUT(!manager.busy(), 5000);
            QVERIFY(core.paused);
            QCOMPARE(manager.filterScenes(manager.scenes(), "parallax", false, 0).size(), 1);
            QCOMPARE(manager.filterScenes(manager.scenes(), "mountain", false, 0).size(), 3);
        }
        QVERIFY(!core.stopped);
        QCOMPARE(core.service->settings().at("fps").get<int>(), 15);
        QVERIFY(!core.service->library().assignments().empty());
    }
    void localSocketRoundTripWhenAvailable() {
        QLocalServer probe;
        if (!probe.listen("AetherWall-Core-probe"))
            QSKIP(
                "Sandbox forbids Unix sockets; transport validation requires an unrestricted host");
        probe.close();
        TestServer core("AetherWall-Core-v1");
        ManagerController manager(false);
        QTRY_COMPARE_WITH_TIMEOUT(manager.scenes().size(), 6, 5000);
    }
    void disconnectedEngineDoesNotInventLibraryOrMetrics() {
        ManagerController manager(false);
        QTest::qWait(800);
        QVERIFY(!manager.connected());
        QVERIFY(manager.scenes().empty());
        QVERIFY(manager.metrics().empty());
    }
};
QTEST_GUILESS_MAIN(ManagerTests)
#include "ManagerTests.moc"
