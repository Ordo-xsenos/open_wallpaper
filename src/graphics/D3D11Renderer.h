#pragma once

#include "graphics/DevicePool.h"
#include "graphics/GpuTimer.h"
#include "graphics/IRenderer.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace aetherwall {
class Logger;

class D3D11Renderer final : public IRenderer {
  public:
    D3D11Renderer(Logger& logger, DevicePool& devices, HMONITOR monitor)
        : logger_(logger), devices_(devices), monitor_(monitor) {}
    ~D3D11Renderer() { shutdown(); }
    D3D11Renderer(const D3D11Renderer&) = delete;
    D3D11Renderer& operator=(const D3D11Renderer&) = delete;
    void initialize(HWND window, const Options& options) override;
    void resize(UINT width, UINT height) override;
    void setScene(std::shared_ptr<const ScenePackage> package) override;
    std::shared_ptr<const TextureMap>
    prepareScene(const std::shared_ptr<const ScenePackage>& package);
    void commitScene(SceneRuntime scene, std::shared_ptr<const TextureMap> textures) noexcept;
    bool animated() const override { return scene_.animated(); }
    PresentResult render(double elapsedSeconds, bool vsync) override;
    bool visible() override;
    FrameSummary gpuTiming() { return gpuTimer_.collect(context_.Get()); }
    void shutdown() noexcept override;

  private:
    void createTarget();
    Logger& logger_;
    DevicePool& devices_;
    HMONITOR monitor_;
    std::shared_ptr<SharedDevice> shared_;
    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> constants_;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blend_;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler_;
    GpuTimer gpuTimer_;
    SceneRuntime scene_{defaultScene()};
    std::shared_ptr<const TextureMap> textures_;
    UINT width_ = 0;
    UINT height_ = 0;
};

} // namespace aetherwall
