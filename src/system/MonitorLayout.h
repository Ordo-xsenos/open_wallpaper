#pragma once

#include <filesystem>
#include <map>
#include <string>

namespace aetherwall {
struct PixelRect {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
};
struct MonitorDescription {
    std::wstring id;
    std::wstring alias;
    PixelRect bounds;
    PixelRect work;
    bool primary = false;
    double refreshHz = 60;
};
std::wstring normalizeMonitorSelector(std::wstring selector);
bool validRect(const PixelRect& rectangle);
bool overlaps(const PixelRect& first, const PixelRect& second);
bool covers(const PixelRect& window, const PixelRect& monitor, int tolerance = 2);
std::filesystem::path assignedScene(const MonitorDescription& monitor,
                                    const std::map<std::wstring, std::filesystem::path>& overrides,
                                    const std::filesystem::path& fallback);
} // namespace aetherwall
