#pragma once

#include "scenes/Scene.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace aetherwall {
struct TextureResource {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    ImageInfo size;
};
using TextureMap = std::map<std::string, TextureResource>;

// Staging map owns all new resources until the complete scene is ready to commit.
TextureMap prepareTextures(ID3D11Device* device, const ScenePackage& package);
} // namespace aetherwall
