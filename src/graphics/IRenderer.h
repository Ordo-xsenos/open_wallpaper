#pragma once

#include "scenes/Scene.h"
#include "system/Options.h"

#include <Windows.h>

namespace aetherwall {
enum class PresentResult { Presented, Occluded, Busy };
class IRenderer {
  public:
    virtual ~IRenderer() = default;
    virtual void initialize(HWND window, const Options& options) = 0;
    virtual void resize(UINT width, UINT height) = 0;
    virtual void setScene(std::shared_ptr<const ScenePackage> package) = 0;
    virtual bool animated() const = 0;
    virtual PresentResult render(double seconds, bool vsync) = 0;
    virtual bool visible() = 0;
    virtual void shutdown() noexcept = 0;
};
} // namespace aetherwall
