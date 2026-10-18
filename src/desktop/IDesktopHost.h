#pragma once

#include <Windows.h>

namespace aetherwall {

class IDesktopHost {
  public:
    virtual ~IDesktopHost() = default;
    virtual void discover() = 0;
    virtual void attach(HWND surface) = 0;
    virtual DPI_AWARENESS_CONTEXT dpiContext() const = 0;
    virtual bool valid(HWND surface) const = 0;
    virtual void hide(HWND surface) noexcept = 0;
    virtual void reset() noexcept = 0;
};

} // namespace aetherwall
