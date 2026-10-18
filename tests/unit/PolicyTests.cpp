#include "system/FrameClock.h"
#include "system/Options.h"
#include "system/RecoveryBudget.h"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>
#include <vector>

namespace aetherwall {
namespace {
using namespace std::chrono_literals;

Options parse(std::initializer_list<std::wstring> arguments) {
    const std::vector<std::wstring> values(arguments);
    return parseOptions(values);
}

TEST(Options, DefaultRunIsFiniteAndDoesNotAttachToDesktop) {
    const auto options = parse({});
    EXPECT_EQ(options.mode, RunMode::Window);
    EXPECT_EQ(options.durationSeconds, 60);
    EXPECT_EQ(options.fps, 30);
    EXPECT_TRUE(options.vsync);
}

TEST(Options, AcceptsExplicitDesktopAndComparisonSettings) {
    const auto options = parse({L"--mode", L"desktop", L"--fps", L"15", L"--duration", L"300",
                                L"--swap-chain", L"blt", L"--no-vsync"});
    EXPECT_EQ(options.mode, RunMode::Desktop);
    EXPECT_EQ(options.swapChain, SwapChainMode::Blt);
    EXPECT_EQ(options.fps, 15);
    EXPECT_EQ(options.durationSeconds, 300);
    EXPECT_FALSE(options.vsync);
}

TEST(Options, AllowsUnlimitedOnlyWhenExplicit) {
    EXPECT_EQ(parse({L"--duration", L"0"}).durationSeconds, 0);
}

TEST(Options, RejectsSoftwareDesktopRendering) {
    EXPECT_THROW(parse({L"--warp", L"--mode", L"desktop"}), std::invalid_argument);
    EXPECT_TRUE(parse({L"--warp"}).warp);
}

TEST(Options, RejectsMalformedAndExcessiveNumericInput) {
    for (const auto* value : {L"", L"-1", L"+15", L"15junk", L"15.0", L"999999999999999999"}) {
        EXPECT_THROW(parse({L"--fps", value}), std::invalid_argument);
    }
    EXPECT_THROW(parse({L"--fps", L"144"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--duration", L"86401"}), std::invalid_argument);
}

TEST(Options, RejectsMissingUnknownAndDuplicateArguments) {
    EXPECT_THROW(parse({L"--mode"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--mode", L"invalid"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--swap-chain", L"invalid"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--unknown"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--fps", L"15", L"--fps", L"30"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--log-dir", L"--mode"}), std::invalid_argument);
}

TEST(Options, AcceptsDirectoryWithSpacesAndHelp) {
    const auto options = parse({L"--log-dir", L"diagnostic reports", L"--help"});
    EXPECT_EQ(options.logDirectory, std::filesystem::path(L"diagnostic reports"));
    EXPECT_TRUE(options.help);
}

TEST(Options, AcceptsExplicitSceneDirectoryOrPackage) {
    EXPECT_EQ(parse({L"--scene", L"my scenes/stars.awscene"}).scenePath,
              std::filesystem::path(L"my scenes/stars.awscene"));
    EXPECT_THROW(parse({L"--scene"}), std::invalid_argument);
    EXPECT_THROW(parse({L"--scene", L"--mode"}), std::invalid_argument);
}

TEST(FrameClock, WaitsUntilDeadlineAndRoundsUpInsteadOfSpinning) {
    const FrameClock::TimePoint start{};
    FrameClock clock(30, start);
    EXPECT_TRUE(clock.due(start));
    clock.frameStarted(start);
    EXPECT_FALSE(clock.due(start + 33ms));
    EXPECT_EQ(clock.wait(start + 33ms), 1ms);
    EXPECT_TRUE(clock.due(start + 34ms));
    EXPECT_EQ(clock.wait(start + 34ms), 0ms);
}

TEST(FrameClock, DropsLateFramesWithoutImmediateCatchUp) {
    const FrameClock::TimePoint start{};
    FrameClock clock(60, start);
    clock.frameStarted(start + 1s);
    EXPECT_FALSE(clock.due(start + 1s));
    EXPECT_EQ(clock.wait(start + 1s), 17ms);
    EXPECT_TRUE(clock.due(start + 1017ms));
}

TEST(FrameClock, RestartMakesFirstFrameDue) {
    const FrameClock::TimePoint start{};
    FrameClock clock(15, start);
    clock.frameStarted(start);
    clock.restart(start + 1ms);
    EXPECT_TRUE(clock.due(start + 1ms));
}

TEST(FrameClock, RejectsInvalidFrameRates) {
    EXPECT_THROW(FrameClock(0, FrameClock::TimePoint{}), std::invalid_argument);
    EXPECT_THROW(FrameClock(-1, FrameClock::TimePoint{}), std::invalid_argument);
}

TEST(FrameClock, BlockingPresentationCountsTowardFramePeriod) {
    const FrameClock::TimePoint start{};
    FrameClock clock(60, start);
    clock.frameStarted(start);
    // Present took a refresh interval: do not add another interval after it returns.
    EXPECT_EQ(clock.wait(start + 17ms), 0ms);
    clock.frameStarted(start + 17ms);
    EXPECT_EQ(clock.wait(start + 17ms), 17ms);
}

TEST(FrameClock, LateFrameDoesNotShortenFollowingFrameInterval) {
    const FrameClock::TimePoint start{};
    FrameClock clock(30, start);
    clock.frameStarted(start);
    clock.frameStarted(start + 40ms);
    EXPECT_FALSE(clock.due(start + 70ms));
    EXPECT_TRUE(clock.due(start + 74ms));
}

TEST(RecoveryBudget, ExhaustionPermanentlyStopsFurtherAttempts) {
    RecoveryBudget budget;
    for (std::size_t index = 0; index < RecoveryBudget::limit; ++index) {
        EXPECT_TRUE(budget.tryConsume());
        EXPECT_EQ(budget.used(), index + 1);
    }
    EXPECT_FALSE(budget.tryConsume());
    EXPECT_FALSE(budget.tryConsume());
    EXPECT_EQ(budget.used(), RecoveryBudget::limit);
}

} // namespace
} // namespace aetherwall
