#include "graphics/D3D11Renderer.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include "scene_ps.h"
#include "scene_vs.h"

#include <array>
#include <iterator>

namespace aetherwall {
using Microsoft::WRL::ComPtr;
namespace {
struct alignas(16) FrameConstants {
    std::array<float, 4> centerAxisX;
    std::array<float, 4> axisYTime;
    std::array<float, 4> uvRect;
    Color tint;
    Color colorA;
    Color colorB;
    std::array<float, 4> parameters;
    std::array<float, 4> flags;
};
static_assert(sizeof(FrameConstants) == 128);
} // namespace

void D3D11Renderer::initialize(HWND window, const Options& options) {
    shutdown();
    RECT client{};
    if (!GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0) {
        throw std::runtime_error("Render surface has no valid client area");
    }
    width_ = static_cast<UINT>(client.right);
    height_ = static_cast<UINT>(client.bottom);
    shared_ = devices_.acquire(monitor_, options);
    device_ = shared_->device;
    context_ = shared_->context;
    logger_.info("Output HWND=" + std::to_string(reinterpret_cast<std::uintptr_t>(window)) +
                 " adapterLUID=" + std::to_string(shared_->luid.HighPart) + ":" +
                 std::to_string(shared_->luid.LowPart));
    auto factory = shared_->factory;
    HRESULT result = S_OK;

    DXGI_SWAP_CHAIN_DESC1 description{};
    description.Width = width_;
    description.Height = height_;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.SampleDesc.Count = 1;
    description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    description.BufferCount = 2;
    description.Scaling = DXGI_SCALING_STRETCH;
    description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    description.SwapEffect = options.swapChain == SwapChainMode::Blt
                                 ? DXGI_SWAP_EFFECT_DISCARD
                                 : DXGI_SWAP_EFFECT_FLIP_DISCARD;
    result = factory->CreateSwapChainForHwnd(device_.Get(), window, &description, nullptr, nullptr,
                                             swapChain_.GetAddressOf());
    if (FAILED(result) && options.swapChain == SwapChainMode::Auto &&
        (result == DXGI_ERROR_INVALID_CALL || result == DXGI_ERROR_UNSUPPORTED)) {
        logger_.warning("Flip swap chain rejected: " + hexCode(static_cast<unsigned long>(result)) +
                        "; trying blt fallback (compatibility must be measured)");
        swapChain_.Reset();
        context_->ClearState();
        context_->Flush();
        description.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        result = factory->CreateSwapChainForHwnd(device_.Get(), window, &description, nullptr,
                                                 nullptr, swapChain_.GetAddressOf());
    }
    checkHr(result, "CreateSwapChainForHwnd");
    checkHr(factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER), "MakeWindowAssociation");
    logger_.info(
        std::string("Swap chain=") +
        (description.SwapEffect == DXGI_SWAP_EFFECT_FLIP_DISCARD ? "flip-discard" : "blt-discard") +
        " size=" + std::to_string(width_) + "x" + std::to_string(height_));

    checkHr(device_->CreateVertexShader(g_scene_vs, std::size(g_scene_vs), nullptr,
                                        vertexShader_.GetAddressOf()),
            "CreateVertexShader");
    checkHr(device_->CreatePixelShader(g_scene_ps, std::size(g_scene_ps), nullptr,
                                       pixelShader_.GetAddressOf()),
            "CreatePixelShader");
    D3D11_BUFFER_DESC buffer{};
    buffer.ByteWidth = sizeof(FrameConstants);
    buffer.Usage = D3D11_USAGE_DEFAULT;
    buffer.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    checkHr(device_->CreateBuffer(&buffer, nullptr, constants_.GetAddressOf()),
            "CreateBuffer(constants)");
    D3D11_BLEND_DESC blending{};
    auto& blend = blending.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D11_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D11_BLEND_ONE;
    blend.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    checkHr(device_->CreateBlendState(&blending, blend_.GetAddressOf()), "CreateBlendState");
    D3D11_SAMPLER_DESC sampling{};
    sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    checkHr(device_->CreateSamplerState(&sampling, sampler_.GetAddressOf()), "CreateSamplerState");
    createTarget();
    gpuTimer_.initialize(device_.Get(), logger_);
    setScene(scene_.source()); // Rebuild last committed scene after Shell/device recovery.
}

void D3D11Renderer::createTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    checkHr(swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf())), "GetBuffer");
    D3D11_RENDER_TARGET_VIEW_DESC description{};
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    description.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    checkHr(device_->CreateRenderTargetView(backBuffer.Get(), &description, target_.GetAddressOf()),
            "CreateRenderTargetView");
}

void D3D11Renderer::resize(UINT width, UINT height) {
    if (!swapChain_ || width == 0 || height == 0 || (width == width_ && height == height_)) {
        return;
    }
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    target_.Reset();
    context_->ClearState();
    context_->Flush();
    checkHr(swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
    width_ = width;
    height_ = height;
    createTarget();
    logger_.info("Render surface resized=" + std::to_string(width) + "x" + std::to_string(height));
}

void D3D11Renderer::setScene(std::shared_ptr<const ScenePackage> package) {
    SceneRuntime next(package);
    if (!device_)
        throw std::runtime_error("Scene upload requires a GPU device");
    auto staged = shared_->upload(package);
    commitScene(std::move(next), std::move(staged));
    logger_.info("Scene committed: " + scene_.package().scene.title +
                 " layers=" + std::to_string(scene_.package().scene.layers.size()) +
                 " animated=" + std::to_string(animated()));
}

std::shared_ptr<const TextureMap>
D3D11Renderer::prepareScene(const std::shared_ptr<const ScenePackage>& package) {
    if (!shared_)
        throw std::runtime_error("Output is recovering; retry when ready");
    return shared_->upload(package);
}
void D3D11Renderer::commitScene(SceneRuntime scene,
                                std::shared_ptr<const TextureMap> textures) noexcept {
    ID3D11ShaderResourceView* empty = nullptr;
    context_->PSSetShaderResources(0, 1, &empty);
    textures_ = std::move(textures);
    scene_ = std::move(scene);
}

PresentResult D3D11Renderer::render(double elapsedSeconds, bool vsync) {
    if (!target_) {
        throw std::runtime_error("Render target unavailable");
    }
    gpuTimer_.begin(context_.Get());
    const D3D11_VIEWPORT viewport{
        0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_), 0.0f, 1.0f};
    context_->RSSetViewports(1, &viewport);
    context_->OMSetRenderTargets(1, target_.GetAddressOf(), nullptr);
    context_->OMSetBlendState(blend_.Get(), nullptr, 0xffffffff);
    context_->ClearRenderTargetView(target_.Get(), scene_.package().scene.canvas.background.data());
    context_->IASetInputLayout(nullptr);
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, constants_.GetAddressOf());
    context_->PSSetConstantBuffers(0, 1, constants_.GetAddressOf());
    context_->PSSetSamplers(0, 1, sampler_.GetAddressOf());
    const auto& scene = scene_.package().scene;
    for (const auto& layer : scene.layers) {
        if (layer.opacity == 0)
            continue;
        ID3D11ShaderResourceView* image = nullptr;
        ImageInfo size{scene.canvas.width, scene.canvas.height};
        if (layer.type == LayerType::Image) {
            const auto& texture = textures_->at(layer.source);
            image = texture.view.Get();
            size = texture.size;
        }
        const auto geometry = layerGeometry(scene.canvas, layer, static_cast<int>(width_),
                                            static_cast<int>(height_), elapsedSeconds, size);
        const FrameConstants frame{
            {geometry.center[0], geometry.center[1], geometry.axisX[0], geometry.axisX[1]},
            {geometry.axisY[0], geometry.axisY[1], static_cast<float>(elapsedSeconds),
             static_cast<float>(scene.canvas.width) / static_cast<float>(scene.canvas.height)},
            geometry.uv,
            layer.tint,
            layer.colorA,
            layer.colorB,
            {layer.opacity, layer.speed, layer.size, static_cast<float>(layer.preset)},
            {static_cast<float>(layer.type), layer.pulse ? 1.0f : 0.0f,
             1.0f / static_cast<float>(scene.canvas.width),
             1.0f / static_cast<float>(scene.canvas.height)}};
        context_->UpdateSubresource(constants_.Get(), 0, nullptr, &frame, 0, 0);
        context_->PSSetShaderResources(0, 1, &image);
        context_->DrawInstanced(
            6, layer.type == LayerType::Particles ? static_cast<UINT>(layer.count) : 1, 0, 0);
    }
    gpuTimer_.end(context_.Get());
    const HRESULT result = swapChain_->Present(vsync ? 1 : 0, DXGI_PRESENT_DO_NOT_WAIT);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING)
        return PresentResult::Busy;
    if (result == DXGI_STATUS_OCCLUDED) {
        return PresentResult::Occluded;
    }
    if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET) {
        logger_.error("Device removed reason=" +
                      hexCode(static_cast<unsigned long>(device_->GetDeviceRemovedReason())));
    }
    checkHr(result, "Present");
    return PresentResult::Presented;
}

bool D3D11Renderer::visible() {
    const HRESULT result = swapChain_->Present(0, DXGI_PRESENT_TEST);
    if (result == DXGI_STATUS_OCCLUDED) {
        return false;
    }
    checkHr(result, "Present(TEST)");
    return true;
}

void D3D11Renderer::shutdown() noexcept {
    if (context_) {
        context_->ClearState();
    }
    gpuTimer_.reset();
    target_.Reset();
    constants_.Reset();
    textures_.reset();
    blend_.Reset();
    sampler_.Reset();
    pixelShader_.Reset();
    vertexShader_.Reset();
    swapChain_.Reset();
    // Flush after releasing views/swap chain, so deferred destruction cannot retain
    // the old flip chain when recovery creates another chain for this HWND.
    if (context_) {
        context_->Flush();
    }
    context_.Reset();
    device_.Reset();
    shared_.reset();
    width_ = 0;
    height_ = 0;
}

} // namespace aetherwall
