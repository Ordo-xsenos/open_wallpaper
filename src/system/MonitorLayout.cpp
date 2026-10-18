#include "system/MonitorLayout.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace aetherwall {
std::wstring normalizeMonitorSelector(std::wstring selector) {
    if (selector.empty() || selector.size() > 512 || selector.starts_with(L"--"))
        throw std::invalid_argument("Invalid monitor selector");
    for (auto& c : selector) {
        if (c < 32)
            throw std::invalid_argument("Control character in monitor selector");
        if (c >= L'A' && c <= L'Z')
            c = static_cast<wchar_t>(c - L'A' + L'a');
    }
    return selector;
}
bool validRect(const PixelRect& rect) { return rect.right > rect.left && rect.bottom > rect.top; }
bool overlaps(const PixelRect& a, const PixelRect& b) {
    return validRect(a) && validRect(b) && std::max(a.left, b.left) < std::min(a.right, b.right) &&
           std::max(a.top, b.top) < std::min(a.bottom, b.bottom);
}
bool covers(const PixelRect& window, const PixelRect& monitor, int tolerance) {
    if (!validRect(window) || !validRect(monitor) || tolerance < 0 || tolerance > 8)
        return false;
    return static_cast<std::int64_t>(window.left) <=
               static_cast<std::int64_t>(monitor.left) + tolerance &&
           static_cast<std::int64_t>(window.top) <=
               static_cast<std::int64_t>(monitor.top) + tolerance &&
           static_cast<std::int64_t>(window.right) >=
               static_cast<std::int64_t>(monitor.right) - tolerance &&
           static_cast<std::int64_t>(window.bottom) >=
               static_cast<std::int64_t>(monitor.bottom) - tolerance;
}
std::filesystem::path assignedScene(const MonitorDescription& monitor,
                                    const std::map<std::wstring, std::filesystem::path>& overrides,
                                    const std::filesystem::path& fallback) {
    const auto byId = overrides.find(normalizeMonitorSelector(monitor.id));
    const auto byAlias = overrides.find(normalizeMonitorSelector(monitor.alias));
    if (byId != overrides.end() && byAlias != overrides.end() && byId != byAlias)
        throw std::invalid_argument("Two selectors assign the same connected monitor");
    if (byId != overrides.end())
        return byId->second;
    return byAlias != overrides.end() ? byAlias->second : fallback;
}
} // namespace aetherwall
