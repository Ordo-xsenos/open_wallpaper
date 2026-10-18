#include "system/PausePolicy.h"

namespace aetherwall {
std::uint32_t pauseReasons(const PauseSettings& settings, const PauseSignals& signals,
                           const PixelRect& monitor) {
    std::uint32_t reasons = 0;
    if (signals.manual)
        reasons |= ManualPause;
    if (signals.locked)
        reasons |= SessionLocked;
    if (signals.suspended)
        reasons |= Suspended;
    if (signals.displayOff)
        reasons |= DisplayOff;
    if (signals.minimized)
        reasons |= Minimized;
    if (settings.battery && signals.onBattery)
        reasons |= Battery;
    const auto& front = signals.foreground;
    if (front.valid && !front.minimized && !front.desktop && !front.ownProcess) {
        if (settings.fullscreen && covers(front.bounds, monitor))
            reasons |= Fullscreen;
        if (settings.maximized && front.maximized && overlaps(front.bounds, monitor))
            reasons |= Maximized;
    }
    return reasons;
}
} // namespace aetherwall
