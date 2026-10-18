#include "graphics/ResourceManager.h"
#include "system/WindowsSupport.h"

#include <webp/decode.h>
#include <wincodec.h>

#include <set>

namespace aetherwall {
using Microsoft::WRL::ComPtr;
namespace {
Bytes decodeImage(IWICImagingFactory* factory, const std::string& name, const Bytes& bytes,
                  ImageInfo expected) {
    const auto stride = static_cast<UINT>(expected.width) * 4;
    Bytes rgba(static_cast<std::size_t>(stride) * static_cast<std::size_t>(expected.height));
    if (name.ends_with(".webp")) {
        if (!WebPDecodeRGBAInto(bytes.data(), bytes.size(), rgba.data(), rgba.size(),
                                static_cast<int>(stride))) {
            throw std::invalid_argument("WebP pixel decoding failed: " + name);
        }
        return rgba;
    }
    ComPtr<IWICStream> stream;
    checkHr(factory->CreateStream(stream.GetAddressOf()), "WIC CreateStream");
    // WIC reads the buffer; the package keeps its bytes alive for the decoder lifetime.
    checkHr(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),
                                         static_cast<DWORD>(bytes.size())),
            "WIC InitializeFromMemory");
    ComPtr<IWICBitmapDecoder> decoder;
    checkHr(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                             decoder.GetAddressOf()),
            "WIC CreateDecoderFromStream");
    UINT frames = 0;
    checkHr(decoder->GetFrameCount(&frames), "WIC GetFrameCount");
    if (frames != 1)
        throw std::invalid_argument("Only single-frame images are supported");
    GUID format{};
    checkHr(decoder->GetContainerFormat(&format), "WIC GetContainerFormat");
    if (format != GUID_ContainerFormatPng && format != GUID_ContainerFormatJpeg)
        throw std::invalid_argument("Unsupported WIC image container");
    ComPtr<IWICBitmapFrameDecode> frame;
    checkHr(decoder->GetFrame(0, frame.GetAddressOf()), "WIC GetFrame");
    UINT width = 0;
    UINT height = 0;
    checkHr(frame->GetSize(&width, &height), "WIC GetSize");
    if (width != static_cast<UINT>(expected.width) || height != static_cast<UINT>(expected.height))
        throw std::invalid_argument("Image dimensions disagree with validated header");
    ComPtr<IWICFormatConverter> converter;
    checkHr(factory->CreateFormatConverter(converter.GetAddressOf()), "WIC CreateFormatConverter");
    checkHr(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA,
                                  WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom),
            "WIC Convert RGBA");
    checkHr(converter->CopyPixels(nullptr, stride, static_cast<UINT>(rgba.size()), rgba.data()),
            "WIC CopyPixels");
    return rgba;
}
} // namespace

TextureMap prepareTextures(ID3D11Device* device, const ScenePackage& package) {
    TextureMap textures;
    if (package.assets.empty())
        return textures;
    ComPtr<IWICImagingFactory> factory;
    checkHr(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                             IID_PPV_ARGS(factory.GetAddressOf())),
            "Create WIC factory");
    std::set<std::string> used;
    for (const auto& layer : package.scene.layers)
        if (layer.type == LayerType::Image)
            used.insert(layer.source);
    for (const auto& [name, bytes] : package.assets) {
        const auto size = package.images.at(name);
        const auto rgba = decodeImage(factory.Get(), name, bytes, size);
        if (!used.contains(name))
            continue; // Validate previews too, without retaining GPU copies.
        D3D11_TEXTURE2D_DESC description{};
        description.Width = static_cast<UINT>(size.width);
        description.Height = static_cast<UINT>(size.height);
        description.MipLevels = 1;
        description.ArraySize = 1;
        description.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        description.SampleDesc.Count = 1;
        description.Usage = D3D11_USAGE_IMMUTABLE;
        description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        const D3D11_SUBRESOURCE_DATA data{rgba.data(), description.Width * 4, 0};
        ComPtr<ID3D11Texture2D> texture;
        checkHr(device->CreateTexture2D(&description, &data, texture.GetAddressOf()),
                "CreateTexture2D");
        TextureResource resource;
        resource.size = size;
        checkHr(
            device->CreateShaderResourceView(texture.Get(), nullptr, resource.view.GetAddressOf()),
            "CreateShaderResourceView");
        textures.emplace(name, std::move(resource));
    }
    return textures;
}
} // namespace aetherwall
