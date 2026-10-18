#pragma once
#include <filesystem>

namespace aetherwall {
std::filesystem::path executableDirectory();
std::filesystem::path defaultDataDirectory();
void configureAutostart(bool enabled, const std::filesystem::path& dataDirectory);
void openManager();
void closeManagerWindows() noexcept;
} // namespace aetherwall
