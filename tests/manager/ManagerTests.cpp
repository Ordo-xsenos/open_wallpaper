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
    void automaticStartupWaitsForProbeAndDoesNotDuplicateCore() {
        TestServer core("", false);
        int starts = 0;
        ManagerController::LaunchHooks hooks;
        hooks.start = [&] {
            ++starts;
            return QString();
        };
        ManagerController manager(
            false, nullptr, [&](const std::string& bytes) { return core.service->handle(bytes); },
            hooks);
        manager.enableAutoStart();
        QTRY_VERIFY_WITH_TIMEOUT(manager.connected(), 5000);
        manager.startCore();
        QCOMPARE(starts, 0);
        QCOMPARE(manager.launchState(), QString("connected"));
    }
    void startupConnectsOnceAndManagerDestructionLeavesCoreAlone() {
        TestServer core("", false);
        bool available = false;
        int starts = 0;
        ManagerController::LaunchHooks hooks;
        hooks.start = [&] {
            ++starts;
            available = true;
            return QString();
        };
        {
            ManagerController manager(
                false, nullptr,
                [&](const std::string& bytes) {
                    if (!available)
                        throw std::runtime_error("Core unavailable");
                    return core.service->handle(bytes);
                },
                hooks);
            manager.enableAutoStart();
            QTRY_VERIFY_WITH_TIMEOUT(manager.connected(), 5000);
            manager.enableAutoStart();
            manager.startCore();
            QCOMPARE(starts, 1);
            QTRY_COMPARE_WITH_TIMEOUT(manager.scenes().size(), 6, 5000);
        }
        QVERIFY(!core.stopped);
    }
    void startupFailuresAreActionableAndOnlyRetryOnRequest() {
        int starts = 0;
        ManagerController::LaunchHooks hooks;
        hooks.start = [&] {
            ++starts;
            return QString("CreateProcessW: 0x00000005");
        };
        ManagerController manager(
            false, nullptr,
            [](const std::string&) -> std::string { throw std::runtime_error("Core unavailable"); },
            hooks);
        manager.enableAutoStart();
        QTRY_COMPARE_WITH_TIMEOUT(manager.launchState(), QString("error"), 2000);
        QVERIFY(manager.launchError().contains("0x00000005"));
        manager.refresh();
        QTest::qWait(200);
        QCOMPARE(starts, 1);
        manager.startCore();
        QCOMPARE(starts, 2);
        QVERIFY(manager.diagnosticText().contains("0x00000005"));
        QVERIFY(manager.diagnosticText().contains("Logs:"));
    }
    void earlyExitAndTimeoutAreDistinctFailures() {
        for (bool exited : {true, false}) {
            ManagerController::LaunchHooks hooks;
            hooks.start = [] { return QString(); };
            hooks.exitCode = [exited]() -> std::optional<quint32> {
                return exited ? std::optional<quint32>(0xc0000135) : std::nullopt;
            };
            hooks.timeoutMs = 100;
            ManagerController manager(
                false, nullptr,
                [](const std::string&) -> std::string {
                    throw std::runtime_error("Core unavailable");
                },
                hooks);
            manager.enableAutoStart();
            QTRY_COMPARE_WITH_TIMEOUT(manager.launchState(), QString("error"), 2000);
            QVERIFY(exited ? manager.launchError().contains("c0000135")
                           : manager.launchError().contains("15"));
        }
    }
    void failedRendererStillProvidesLibraryAndDiagnostics() {
        TestServer core("", false);
        ManagerController manager(false, nullptr, [&](const std::string& bytes) {
            auto reply = aetherwall::parseJson(core.service->handle(bytes));
            if (aetherwall::parseJson(bytes).at("command") == "getStatus") {
                reply["result"]["renderState"] = "failed";
                reply["result"]["renderError"] = "No supported icon host";
            }
            return reply.dump();
        });
        QTRY_COMPARE_WITH_TIMEOUT(manager.scenes().size(), 6, 5000);
        QVERIFY(manager.connected());
        QCOMPARE(manager.status().value("renderState").toString(), QString("failed"));
        QVERIFY(manager.diagnosticText().contains("No supported icon host"));
    }
    void localSocketRoundTripWhenAvailable() {
#ifdef _WIN32
        QSKIP("Unix socket harness; Windows uses the production secured Named Pipe transport");
#endif
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
