#include "diagnostics/FrameMetrics.h"
#include "system/MonitorLayout.h"
#include "system/Options.h"
#include "system/PausePolicy.h"
#include "system/RecoveryPolicy.h"
#include <chrono>
#include <gtest/gtest.h>
#include <limits>
#include <stdexcept>
#include <vector>

namespace aetherwall {
namespace {
using namespace std::chrono_literals;
Options parseM2(std::initializer_list<std::wstring> args) {
    return parseOptions(std::vector<std::wstring>(args));
}
const PixelRect left{-1920, -240, 0, 840};
const PixelRect right{0, 0, 2560, 1440};
TEST(MonitorLayout, SupportsNegativeCoordinatesAndTouchingEdges) {
    EXPECT_TRUE(validRect(left));
    EXPECT_FALSE(overlaps(left, right));
    EXPECT_TRUE(overlaps(left, {-10, 0, 10, 200}));
    EXPECT_FALSE(validRect({0, 0, 0, 1080}));
    EXPECT_FALSE(overlaps({}, right));
}
TEST(MonitorLayout, CoverageToleranceIsBoundedAndDoesNotOverflow) {
    EXPECT_TRUE(covers({-1918, -238, -2, 838}, left));
    EXPECT_FALSE(covers({-1917, -240, 0, 840}, left));
    EXPECT_FALSE(covers(left, left, -1));
    EXPECT_FALSE(covers(left, left, 9));
    const PixelRect extreme{std::numeric_limits<int>::min(), 0, std::numeric_limits<int>::max(), 1};
    EXPECT_TRUE(covers(extreme, extreme));
    EXPECT_FALSE(covers({}, right));
}
TEST(MonitorLayout, ResolvesPersistentIdsOrAliasesAndRejectsAmbiguity) {
    MonitorDescription monitor;
    monitor.id = L"monitor:ABC";
    monitor.alias = L"DISPLAY2";
    std::map<std::wstring, std::filesystem::path> assignments{{L"display2", "rain"}};
    EXPECT_EQ(assignedScene(monitor, assignments, "stars"), "rain");
    assignments.clear();
    assignments[L"monitor:abc"] = "mountains";
    EXPECT_EQ(assignedScene(monitor, assignments, "stars"), "mountains");
    assignments[L"display2"] = "still";
    EXPECT_THROW(assignedScene(monitor, assignments, "stars"), std::invalid_argument);
    EXPECT_EQ(assignedScene(monitor, {}, "stars"), "stars");
}
TEST(MonitorLayout, NormalizationPreservesDevicePathPunctuation) {
    EXPECT_EQ(normalizeMonitorSelector(L"\\\\?\\DISPLAY#ABC"), L"\\\\?\\display#abc");
    EXPECT_THROW(normalizeMonitorSelector(L""), std::invalid_argument);
    EXPECT_THROW(normalizeMonitorSelector(L"--scene"), std::invalid_argument);
    EXPECT_THROW(normalizeMonitorSelector(L"DISPLAY\n1"), std::invalid_argument);
    EXPECT_THROW(normalizeMonitorSelector(std::wstring(513, L'a')), std::invalid_argument);
}
TEST(PausePolicy, FullscreenStopsOnlyTheCoveredMonitor) {
    PauseSignals signals;
    signals.foreground.bounds = left;
    signals.foreground.valid = true;
    EXPECT_EQ(pauseReasons({}, signals, left), Fullscreen);
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
    signals.foreground.bounds = {-1920, -240, 2560, 1440};
    EXPECT_EQ(pauseReasons({}, signals, right), Fullscreen);
}
TEST(PausePolicy, ExcludesShellOwnWindowsAndMinimizedForeground) {
    PauseSignals signals;
    signals.foreground = {right, true, false, false, true, false};
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
    signals.foreground.desktop = false;
    signals.foreground.ownProcess = true;
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
    signals.foreground.ownProcess = false;
    signals.foreground.minimized = true;
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
    signals.foreground.minimized = false;
    signals.foreground.valid = false;
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
}
TEST(PausePolicy, MaximizedIsIndependentAndChecksIntersection) {
    PauseSettings settings;
    settings.fullscreen = false;
    settings.maximized = true;
    PauseSignals signals;
    signals.foreground = {{0, 0, 2560, 1400}, true, false, true, false, false};
    EXPECT_EQ(pauseReasons(settings, signals, right), Maximized);
    EXPECT_EQ(pauseReasons(settings, signals, left), 0u);
    settings.maximized = false;
    EXPECT_EQ(pauseReasons(settings, signals, right), 0u);
}
TEST(PausePolicy, IndependentBlockersMustAllClearBeforeResume) {
    PauseSignals signals;
    signals.manual = signals.locked = signals.suspended = signals.onBattery = signals.displayOff =
        signals.minimized = true;
    const auto expected =
        ManualPause | SessionLocked | Suspended | Battery | DisplayOff | Minimized;
    EXPECT_EQ(pauseReasons({}, signals, right), static_cast<std::uint32_t>(expected));
    signals.manual = false;
    EXPECT_NE(pauseReasons({}, signals, right), 0u);
    signals = {};
    EXPECT_EQ(pauseReasons({}, signals, right), 0u);
}
TEST(PausePolicy, BatteryCanBeDisabledWithoutDisablingLockOrSleep) {
    PauseSettings settings;
    settings.battery = false;
    PauseSignals signals;
    signals.onBattery = true;
    EXPECT_EQ(pauseReasons(settings, signals, right), 0u);
    signals.locked = true;
    signals.displayOff = true;
    EXPECT_EQ(pauseReasons(settings, signals, right),
              static_cast<std::uint32_t>(SessionLocked | DisplayOff));
}
TEST(M2Options, PresetsGpuAndPauseOverrides) {
    EXPECT_EQ(parseM2({L"--preset", L"eco"}).fps, 15);
    EXPECT_EQ(parseM2({L"--preset", L"balanced"}).fps, 30);
    const auto options =
        parseM2({L"--preset", L"smooth", L"--gpu", L"high-performance", L"--pause-fullscreen",
                 L"off", L"--pause-maximized", L"on", L"--pause-battery", L"off"});
    EXPECT_EQ(options.fps, 60);
    EXPECT_EQ(options.gpu, GpuPreference::HighPerformance);
    EXPECT_FALSE(options.pause.fullscreen);
    EXPECT_TRUE(options.pause.maximized);
    EXPECT_FALSE(options.pause.battery);
    EXPECT_EQ(parseM2({L"--gpu", L"power-saving"}).gpu, GpuPreference::PowerSaving);
    EXPECT_EQ(parseM2({L"--gpu", L"auto"}).gpu, GpuPreference::Auto);
    EXPECT_TRUE(parseM2({L"--list-monitors"}).listMonitors);
}
TEST(M2Options, RejectsConflictingAndUnknownPolicies) {
    EXPECT_THROW(parseM2({L"--preset", L"eco", L"--fps", L"30"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--fps", L"30", L"--preset", L"eco"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--preset", L"fast"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--gpu", L"nvidia"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--pause-battery", L"maybe"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--all-monitors", L"--all-monitors"}), std::invalid_argument);
}
TEST(M2Options, RepeatableAssignmentsAllowSpacesAndNormalizeKeys) {
    const auto options =
        parseM2({L"--mode", L"desktop", L"--monitor-scene", L"DISPLAY1", L"my scenes/stars",
                 L"--monitor-scene", L"DISPLAY2", L"rain.awscene"});
    EXPECT_EQ(options.monitorScenes.size(), 2u);
    EXPECT_EQ(options.monitorScenes.at(L"display1"), "my scenes/stars");
    EXPECT_TRUE(
        parseM2({L"--all-monitors", L"--monitor-scene", L"DISPLAY1", L"stars"}).allMonitors);
}
TEST(M2Options, RejectsAmbiguousOrIncompleteMonitorAssignments) {
    EXPECT_THROW(parseM2({L"--monitor-scene", L"DISPLAY1", L"stars"}), std::invalid_argument);
    EXPECT_THROW(parseM2({L"--all-monitors", L"--monitor-scene", L"DISPLAY1"}),
                 std::invalid_argument);
    EXPECT_THROW(parseM2({L"--all-monitors", L"--monitor-scene", L"DISPLAY1", L"--scene"}),
                 std::invalid_argument);
    EXPECT_THROW(parseM2({L"--all-monitors", L"--monitor-scene", L"DISPLAY1", L"stars",
                          L"--monitor-scene", L"display1", L"rain"}),
                 std::invalid_argument);
    std::vector<std::wstring> args{L"--mode", L"desktop"};
    for (int i = 0; i < 17; ++i) {
        args.push_back(L"--monitor-scene");
        args.push_back(L"DISPLAY" + std::to_wstring(i));
        args.push_back(L"stars");
    }
    EXPECT_THROW(parseOptions(args), std::invalid_argument);
}
TEST(FrameMetrics, UsesNearestRankP95AndIgnoresInvalidSamples) {
    FrameMetrics metrics;
    EXPECT_EQ(metrics.summary().samples, 0u);
    for (int i = 1; i <= 100; ++i)
        metrics.add(i);
    metrics.add(-1);
    metrics.add(std::numeric_limits<double>::quiet_NaN());
    metrics.add(std::numeric_limits<double>::infinity());
    EXPECT_DOUBLE_EQ(metrics.summary().averageMs, 50.5);
    EXPECT_DOUBLE_EQ(metrics.summary().p95Ms, 95);
    EXPECT_EQ(metrics.summary().samples, 100u);
}
TEST(FrameMetrics, RetainsOnlyTheLatestSixHundredSamples) {
    FrameMetrics metrics;
    for (int i = 0; i < 600; ++i)
        metrics.add(999);
    for (int i = 0; i < 600; ++i)
        metrics.add(1);
    EXPECT_EQ(metrics.summary().samples, 600u);
    EXPECT_DOUBLE_EQ(metrics.summary().p95Ms, 1);
    EXPECT_DOUBLE_EQ(metrics.summary().averageMs, 1);
    metrics.clear();
    EXPECT_EQ(metrics.summary().samples, 0u);
    metrics.add(7);
    EXPECT_DOUBLE_EQ(metrics.summary().p95Ms, 7);
}
TEST(RecoveryPolicy, StopsAfterThreeConsecutiveFailures) {
    RecoveryPolicy policy;
    const RecoveryPolicy::Clock::time_point start{};
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(policy.beginAttempt(start + i * 2s));
        policy.completed(false);
    }
    EXPECT_FALSE(policy.beginAttempt(start + 40s));
}
TEST(RecoveryPolicy, SuccessfulRebuildResetsConsecutiveFailures) {
    RecoveryPolicy policy;
    const RecoveryPolicy::Clock::time_point start{};
    EXPECT_TRUE(policy.beginAttempt(start));
    policy.completed(false);
    EXPECT_TRUE(policy.beginAttempt(start + 2s));
    policy.completed(false);
    EXPECT_TRUE(policy.beginAttempt(start + 4s));
    policy.completed(true);
    EXPECT_TRUE(policy.beginAttempt(start + 6s));
    policy.completed(false);
    EXPECT_TRUE(policy.beginAttempt(start + 8s));
    policy.completed(true);
}
TEST(RecoveryPolicy, LimitsRestartStormsButAllowsLongLivedReconnects) {
    RecoveryPolicy policy;
    const RecoveryPolicy::Clock::time_point start{};
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(policy.beginAttempt(start + i * 1s));
        policy.completed(true);
    }
    EXPECT_FALSE(policy.beginAttempt(start + 29s));
    EXPECT_TRUE(policy.beginAttempt(start + 30s));
    policy.completed(true);
    for (int i = 1; i < 20; ++i) {
        EXPECT_TRUE(policy.beginAttempt(start + i * 60s));
        policy.completed(true);
    }
}
} // namespace
} // namespace aetherwall
