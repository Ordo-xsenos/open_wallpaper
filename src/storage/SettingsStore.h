#pragma once
#include "ipc/Protocol.h"
#include <filesystem>

namespace aetherwall {
Json defaultSettings();
Json validateSettings(const Json& value);
void atomicWrite(const std::filesystem::path& path, std::string_view bytes);
class SettingsStore {
  public:
    explicit SettingsStore(std::filesystem::path directory);
    Json load();
    void save(const Json& settings);
    const std::string& warning() const { return warning_; }

  private:
    std::filesystem::path path_;
    std::string warning_;
};
} // namespace aetherwall
