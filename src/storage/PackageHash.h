#pragma once
#include <filesystem>
#include <string>

namespace aetherwall {
std::string packageSha256(const std::filesystem::path& path, std::uintmax_t expectedSize);
} // namespace aetherwall
