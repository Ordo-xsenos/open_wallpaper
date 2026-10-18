#pragma once

#include "system/MonitorLayout.h"

#include <cstdint>

namespace aetherwall {
struct PauseSettings {
    bool fullscreen = true;
    bool maximized = false;
    bool battery = true;
};
struct ForegroundState {
    PixelRect bounds;
    bool valid = false;
    bool minimized = false;
    bool maximized = false;
    bool desktop = false;
    bool ownProcess = false;
};
struct PauseSignals {
    bool manual = false;
    bool locked = false;
    bool suspended = false;
    bool onBattery = false;
    bool displayOff = false;
    bool minimized = false;
    ForegroundState foreground;
};
enum PauseReason : std::uint32_t {
    ManualPause = 1,
    SessionLocked = 2,
    Suspended = 4,
    Battery = 8,
    DisplayOff = 16,
    Fullscreen = 32,
    Maximized = 64,
    Minimized = 128
};
std::uint32_t pauseReasons(const PauseSettings& settings, const PauseSignals& signals,
                           const PixelRect& monitor);
} // namespace aetherwall
