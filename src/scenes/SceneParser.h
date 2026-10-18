#pragma once

#include "scenes/Scene.h"

#include <string_view>

namespace aetherwall {
SceneDefinition parseSceneManifest(std::string_view manifest);
std::string normalizedAssetPath(std::string_view path);
} // namespace aetherwall
