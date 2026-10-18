#include "system/PowerAndForegroundService.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include <array>
#include <dwmapi.h>
#include <powersetting.h>

namespace aetherwall {
PowerAndForegroundService::~PowerAndForegroundService() { stop(); }
void PowerAndForegroundService::stop() noexcept {
    for (auto hook : hooks_)
        if (hook)
            UnhookWinEvent(hook);
    if (displayNotification_)
        UnregisterPowerSettingNotification(displayNotification_);
    hooks_ = {};
    displayNotification_ = nullptr;
}
void CALLBACK PowerAndForegroundService::winEvent(HWINEVENTHOOK, DWORD eventId, HWND window,
                                                  LONG object, LONG child, DWORD, DWORD) {
    if (eventId == EVENT_SYSTEM_FOREGROUND ||
        (window == GetForegroundWindow() && object == OBJID_WINDOW && child == CHILDID_SELF))
        PostThreadMessageW(GetCurrentThreadId(), changedMessage, 0, 0);
}
void PowerAndForegroundService::start(HWND control) {
    hooks_[0] = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr, winEvent,
                                0, 0, WINEVENT_OUTOFCONTEXT);
    hooks_[1] = SetWinEventHook(EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE, nullptr,
                                winEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (!hooks_[0] || !hooks_[1])
        logger_.warning("Foreground hooks partially unavailable; 250 ms polling remains active");
    displayNotification_ = RegisterPowerSettingNotification(control, &GUID_CONSOLE_DISPLAY_STATE,
                                                            DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!displayNotification_)
        logger_.warning("Console display-power notification unavailable; individual panel sleep is "
                        "not inferred");
}
PauseSignals PowerAndForegroundService::snapshot(bool locked, bool suspended) {
    PauseSignals signals;
    signals.locked = locked;
    signals.suspended = suspended;
    signals.displayOff = displayOff_;
    SYSTEM_POWER_STATUS power{};
    if (GetSystemPowerStatus(&power))
        signals.onBattery = power.ACLineStatus == 0;
    const HWND window = GetForegroundWindow();
    if (!window || !IsWindowVisible(window))
        return signals;
    auto& state = signals.foreground;
    state.minimized = IsIconic(window) != FALSE;
    state.maximized = IsZoomed(window) != FALSE;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    state.ownProcess = pid == GetCurrentProcessId();
    std::array<wchar_t, 128> name{};
    GetClassNameW(window, name.data(), static_cast<int>(name.size()));
    const std::wstring className(name.data());
    state.desktop = window == GetShellWindow() || className == L"Progman" ||
                    className == L"WorkerW" || className == L"Shell_TrayWnd" ||
                    className == L"Shell_SecondaryTrayWnd";
    DWORD cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))) &&
        cloaked)
        return signals;
    const ScopedDpiContext physical(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RECT bounds{};
    if (FAILED(
            DwmGetWindowAttribute(window, DWMWA_EXTENDED_FRAME_BOUNDS, &bounds, sizeof(bounds))) &&
        !GetWindowRect(window, &bounds))
        return signals;
    state.bounds = {static_cast<int>(bounds.left), static_cast<int>(bounds.top),
                    static_cast<int>(bounds.right), static_cast<int>(bounds.bottom)};
    state.valid = validRect(state.bounds);
    return signals;
}
} // namespace aetherwall
