#pragma once

#include "graphics/ResourceManager.h"
#include "system/Options.h"
#include <dxgi1_6.h>
#include <memory>
#include <vector>

namespace aetherwall {
class Logger;
struct SharedDevice {
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    LUID luid{};
    bool warp = false;
    std::map<const ScenePackage*, std::weak_ptr<const TextureMap>> textures;
    std::shared_ptr<const TextureMap> upload(const std::shared_ptr<const ScenePackage>& package);
};
// Owned by Core; every immediate-context call stays on its message-loop thread.
class DevicePool {
  public:
    explicit DevicePool(Logger& logger) : logger_(logger) {}
    std::shared_ptr<SharedDevice> acquire(HMONITOR monitor, const Options& options);
    void clear() { devices_.clear(); }

  private:
    Logger& logger_;
    std::vector<std::shared_ptr<SharedDevice>> devices_;
};
} // namespace aetherwall
