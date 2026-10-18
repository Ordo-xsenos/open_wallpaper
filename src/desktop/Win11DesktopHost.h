#pragma once

#include "desktop/IDesktopHost.h"
#include "system/MonitorLayout.h"

namespace aetherwall {
class Logger;

class Windows11DesktopHost final : public IDesktopHost {
  public:
    explicit Windows11DesktopHost(Logger& logger) : logger_(logger) {}
    void discover() override;
    void attach(HWND surface) override;
    void position(HWND surface, const PixelRect& monitor);
    DPI_AWARENESS_CONTEXT dpiContext() const override;
    bool valid(HWND surface) const override;
    void hide(HWND surface) noexcept override;
    void reset() noexcept override;

  private:
    Logger& logger_;
    HWND host_ = nullptr; // Borrowed Shell windows; never destroy them.
    HWND iconHost_ = nullptr;
    HWND iconView_ = nullptr;
    DWORD shellProcess_ = 0;
};

} // namespace aetherwall
