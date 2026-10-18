#pragma once

#include "scenes/Scene.h"

#include <filesystem>
#include <span>

namespace aetherwall {
ImageInfo inspectImage(std::string_view filename, std::span<const std::uint8_t> bytes);
std::shared_ptr<const ScenePackage> loadScenePackage(const std::filesystem::path& path);
} // namespace aetherwall
