#pragma once
#include "system/PausePolicy.h"
#include <Windows.h>
#include <array>

namespace aetherwall {
class Logger;
class PowerAndForegroundService {
  public:
    static constexpr UINT changedMessage = WM_APP + 23;
    explicit PowerAndForegroundService(Logger& logger) : logger_(logger) {}
    ~PowerAndForegroundService();
    void start(HWND control);
    void stop() noexcept;
    PauseSignals snapshot(bool locked, bool suspended);
    void displayState(DWORD state) { displayOff_ = state == 0; }

  private:
    static void CALLBACK winEvent(HWINEVENTHOOK, DWORD, HWND, LONG, LONG, DWORD, DWORD);
    Logger& logger_;
    std::array<HWINEVENTHOOK, 2> hooks_{};
    HPOWERNOTIFY displayNotification_ = nullptr;
    bool displayOff_ = false;
};
} // namespace aetherwall
