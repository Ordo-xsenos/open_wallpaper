#pragma once
#include <Windows.h>
#include <shellapi.h>

namespace aetherwall {
class TrayController {
  public:
    static constexpr UINT callbackMessage = WM_APP + 42;
    static constexpr UINT openCommand = 4101, pauseCommand = 4102, reloadCommand = 4103,
                          stopCommand = 4104;
    ~TrayController();
    void install(HWND owner);
    void remove() noexcept;
    void restore();
    void setState(bool paused, bool russian) {
        paused_ = paused;
        russian_ = russian;
    }
    void handle(LPARAM event) noexcept;

  private:
    NOTIFYICONDATAW icon_{};
    bool installed_ = false, paused_ = false, russian_ = true;
};
} // namespace aetherwall
