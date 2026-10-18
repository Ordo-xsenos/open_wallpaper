#include "storage/SettingsStore.h"
#include <fstream>
#include <random>
#include <stdexcept>

namespace aetherwall {
Json defaultSettings() {
    return {{"schemaVersion", 2},
            {"fps", 30},
            {"gpu", "auto"},
            {"pauseFullscreen", true},
            {"pauseMaximized", false},
            {"pauseBattery", true},
            {"language", "ru"},
            {"autostart", false},
            {"catalogBaseUrl", ""},
            {"catalogAssetOrigins", Json::array()},
            {"allowLocalCatalog", false}};
}
Json validateSettings(const Json& value) {
    onlyFields(value, {"schemaVersion", "fps", "gpu", "pauseFullscreen", "pauseMaximized",
                       "pauseBattery", "language", "autostart", "catalogBaseUrl",
                       "catalogAssetOrigins", "allowLocalCatalog"});
    if (value.size() != defaultSettings().size() ||
        !value.at("schemaVersion").is_number_integer() || value.at("schemaVersion") != 2)
        throw std::invalid_argument("Unsupported settings schema");
    if (!value.at("fps").is_number_integer() ||
        (value.at("fps") != 15 && value.at("fps") != 30 && value.at("fps") != 60))
        throw std::invalid_argument("FPS must be 15, 30 or 60");
    const auto gpu = boundedText(value.at("gpu"));
    if (gpu != "auto" && gpu != "power-saving" && gpu != "high-performance")
        throw std::invalid_argument("Invalid GPU preference");
    const auto language = boundedText(value.at("language"));
    if (language != "ru" && language != "en")
        throw std::invalid_argument("Unsupported language");
    for (const auto* key :
         {"pauseFullscreen", "pauseMaximized", "pauseBattery", "autostart", "allowLocalCatalog"})
        if (!value.at(key).is_boolean())
            throw std::invalid_argument("Expected boolean setting");
    boundedText(value.at("catalogBaseUrl"), 2048, true);
    const auto& origins = value.at("catalogAssetOrigins");
    if (!origins.is_array() || origins.size() > 8)
        throw std::invalid_argument("Expected at most eight catalog asset origins");
    for (const auto& origin : origins)
        boundedText(origin, 2048);
    return value;
}
SettingsStore::SettingsStore(std::filesystem::path directory)
    : path_(std::move(directory) / "settings.json") {}
Json SettingsStore::load() {
    auto read = [](const std::filesystem::path& path) {
        if (std::filesystem::file_size(path) > 16384)
            throw std::runtime_error("Settings file too large");
        std::ifstream file(path, std::ios::binary);
        if (!file)
            throw std::runtime_error("Cannot open settings");
        auto settings = parseJson(std::string(std::istreambuf_iterator<char>(file), {}), 16384);
        if (settings.is_object() && settings.value("schemaVersion", 0) == 1) {
            onlyFields(settings, {"schemaVersion", "fps", "gpu", "pauseFullscreen",
                                  "pauseMaximized", "pauseBattery", "language", "autostart"});
            if (settings.size() != 8)
                throw std::invalid_argument("Incomplete legacy settings");
            auto migrated = defaultSettings();
            for (const auto& field : settings.items())
                if (field.key() != "schemaVersion")
                    migrated[field.key()] = field.value();
            settings = std::move(migrated);
        }
        return validateSettings(settings);
    };
    if (!std::filesystem::exists(path_) &&
        !std::filesystem::exists(
            std::filesystem::path(path_.native() + std::filesystem::path(".bak").native())))
        return defaultSettings();
    try {
        return read(path_);
    } catch (const std::exception&) {
        warning_ = "Primary settings invalid; using backup or defaults. Original files retained.";
    }
    try {
        return read(std::filesystem::path(path_.native() + std::filesystem::path(".bak").native()));
    } catch (const std::exception&) {
        return defaultSettings();
    }
}
void SettingsStore::save(const Json& settings) {
    const auto bytes = validateSettings(settings).dump(2) + "\n";
    // Never replace a valid backup with a known-corrupt primary during recovery.
    if (!warning_.empty() && std::filesystem::exists(path_)) {
        const auto rejected = std::filesystem::path(
            path_.native() +
            std::filesystem::path(".invalid-" + std::to_string(std::random_device{}())).native());
        std::filesystem::rename(path_, rejected);
    }
    atomicWrite(path_, bytes);
    warning_.clear();
}
} // namespace aetherwall
