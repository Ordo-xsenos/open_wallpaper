#include "graphics/DevicePool.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"
#include <algorithm>
#include <set>

namespace aetherwall {
using Microsoft::WRL::ComPtr;
namespace {
bool sameLuid(LUID a, LUID b) { return a.LowPart == b.LowPart && a.HighPart == b.HighPart; }
bool drivesMonitor(IDXGIAdapter1* adapter, HMONITOR monitor) {
    for (UINT i = 0;; ++i) {
        ComPtr<IDXGIOutput> output;
        const HRESULT result = adapter->EnumOutputs(i, output.GetAddressOf());
        if (result == DXGI_ERROR_NOT_FOUND)
            return false;
        checkHr(result, "EnumOutputs");
        DXGI_OUTPUT_DESC description{};
        checkHr(output->GetDesc(&description), "Output GetDesc");
        if (description.AttachedToDesktop && description.Monitor == monitor)
            return true;
    }
}
std::size_t textureBytes(const TextureMap& textures) {
    std::size_t total = 0;
    for (const auto& [name, resource] : textures) {
        (void)name;
        total += static_cast<std::size_t>(resource.size.width) *
                 static_cast<std::size_t>(resource.size.height) * 4;
    }
    return total;
}
} // namespace
std::shared_ptr<const TextureMap>
SharedDevice::upload(const std::shared_ptr<const ScenePackage>& package) {
    if (const auto it = textures.find(package.get()); it != textures.end())
        if (auto shared = it->second.lock())
            return shared;
    std::size_t liveBytes = 0;
    for (auto it = textures.begin(); it != textures.end();) {
        if (auto live = it->second.lock()) {
            liveBytes += textureBytes(*live);
            ++it;
        } else
            it = textures.erase(it);
    }
    std::set<std::string> used;
    for (const auto& layer : package->scene.layers)
        if (layer.type == LayerType::Image)
            used.insert(layer.source);
    std::size_t requested = 0;
    for (const auto& name : used) {
        const auto size = package->images.at(name);
        requested +=
            static_cast<std::size_t>(size.width) * static_cast<std::size_t>(size.height) * 4;
    }
    constexpr std::size_t budget = 256 * 1024 * 1024;
    if (liveBytes > budget || requested > budget - liveBytes)
        throw std::runtime_error(
            "Shared GPU texture budget (256 MiB including staged scenes) exceeded");
    auto staged = std::make_shared<const TextureMap>(prepareTextures(device.Get(), *package));
    textures[package.get()] = staged;
    return staged;
}
std::shared_ptr<SharedDevice> DevicePool::acquire(HMONITOR monitor, const Options& options) {
    ComPtr<IDXGIFactory6> factory;
    checkHr(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf())),
            "CreateDXGIFactory1(factory6)");
    std::vector<ComPtr<IDXGIAdapter1>> adapters;
    if (!options.warp) {
        const auto preference = options.gpu == GpuPreference::HighPerformance
                                    ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE
                                    : DXGI_GPU_PREFERENCE_MINIMUM_POWER;
        for (UINT i = 0;; ++i) {
            ComPtr<IDXGIAdapter1> adapter;
            const HRESULT result = factory->EnumAdapterByGpuPreference(
                i, preference, IID_PPV_ARGS(adapter.GetAddressOf()));
            if (result == DXGI_ERROR_NOT_FOUND)
                break;
            checkHr(result, "EnumAdapterByGpuPreference");
            DXGI_ADAPTER_DESC1 description{};
            checkHr(adapter->GetDesc1(&description), "Adapter GetDesc1");
            if (!(description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE))
                adapters.push_back(std::move(adapter));
        }
        if (options.gpu == GpuPreference::Auto) {
            // Preserve minimum-power ordering among adapters driving the selected monitor.
            std::stable_partition(adapters.begin(), adapters.end(), [monitor](const auto& adapter) {
                return drivesMonitor(adapter.Get(), monitor);
            });
        }
    } else
        adapters.emplace_back();
    for (const auto& adapter : adapters) {
        DXGI_ADAPTER_DESC1 description{};
        if (adapter)
            checkHr(adapter->GetDesc1(&description), "Adapter GetDesc1");
        for (const auto& existing : devices_)
            if (existing->warp == options.warp &&
                (options.warp || sameLuid(existing->luid, description.AdapterLuid)))
                return existing;
        auto shared = std::make_shared<SharedDevice>();
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        const D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
        D3D_FEATURE_LEVEL actual{};
        auto create = [&] {
            shared->device.Reset();
            shared->context.Reset();
            return D3D11CreateDevice(
                adapter.Get(), options.warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN,
                nullptr, flags, &level, 1, D3D11_SDK_VERSION, shared->device.GetAddressOf(),
                &actual, shared->context.GetAddressOf());
        };
        HRESULT result = create();
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
            logger_.warning("D3D debug layer unavailable; retrying without it");
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            result = create();
        }
        if (FAILED(result)) {
            logger_.warning("GPU candidate rejected: " +
                            hexCode(static_cast<unsigned long>(result)));
            continue;
        }
        shared->luid = description.AdapterLuid;
        shared->warp = options.warp;
        checkHr(factory.As(&shared->factory), "QueryInterface(factory2)");
        logger_.info("GPU=" + utf8(description.Description) +
                     " LUID=" + std::to_string(description.AdapterLuid.HighPart) + ":" +
                     std::to_string(description.AdapterLuid.LowPart) +
                     " featureLevel=" + hexCode(static_cast<unsigned long>(actual)) +
                     " software=" + std::to_string(options.warp) + " drivesMonitor=" +
                     std::to_string(adapter && drivesMonitor(adapter.Get(), monitor)));
        if (adapter) {
            LARGE_INTEGER version{};
            if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &version))) {
                const auto bits = static_cast<unsigned long long>(version.QuadPart);
                logger_.info("GPU driver=" + std::to_string((bits >> 48) & 0xffff) + "." +
                             std::to_string((bits >> 32) & 0xffff) + "." +
                             std::to_string((bits >> 16) & 0xffff) + "." +
                             std::to_string(bits & 0xffff));
            } else {
                logger_.warning("GPU driver version unavailable from DXGI");
            }
            if (!drivesMonitor(adapter.Get(), monitor))
                logger_.warning(
                    "GPU does not drive this output; cross-adapter copies may increase power use");
        }
        devices_.push_back(shared);
        return shared;
    }
    throw std::runtime_error("No selected hardware adapter supports D3D11 feature level 11_0");
}
} // namespace aetherwall
