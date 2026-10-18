#include "system/MonitorService.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include <algorithm>
#include <set>

namespace aetherwall {
namespace {
struct Enumeration {
    std::vector<Monitor> monitors;
    bool failed = false;
};
BOOL CALLBACK collectMonitor(HMONITOR handle, HDC, LPRECT, LPARAM parameter) {
    auto& result = *reinterpret_cast<Enumeration*>(parameter);
    try {
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(handle, reinterpret_cast<MONITORINFO*>(&info)))
            throwLastError("GetMonitorInfoW");
        if (result.monitors.size() >= 16)
            throw std::runtime_error("At most 16 outputs are supported");
        Monitor monitor;
        monitor.handle = handle;
        monitor.alias = info.szDevice;
        if (monitor.alias.starts_with(L"\\\\.\\"))
            monitor.alias.erase(0, 4);
        monitor.id = L"gdi:" + monitor.alias;
        monitor.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        monitor.bounds = {
            static_cast<int>(info.rcMonitor.left), static_cast<int>(info.rcMonitor.top),
            static_cast<int>(info.rcMonitor.right), static_cast<int>(info.rcMonitor.bottom)};
        monitor.work = {static_cast<int>(info.rcWork.left), static_cast<int>(info.rcWork.top),
                        static_cast<int>(info.rcWork.right), static_cast<int>(info.rcWork.bottom)};
        if (!validRect(monitor.bounds))
            throw std::runtime_error("Invalid monitor bounds");
        if (!validRect(monitor.work))
            monitor.work = monitor.bounds;
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0) &&
            mode.dmDisplayFrequency > 1)
            monitor.refreshHz = static_cast<double>(mode.dmDisplayFrequency);
        result.monitors.push_back(std::move(monitor));
        return TRUE;
    } catch (...) {
        result.failed = true;
        return FALSE;
    }
}
void assignPersistentIds(std::vector<Monitor>& monitors, Logger& logger) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        UINT32 pathCount = 0, modeCount = 0;
        LONG result = GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount);
        if (result != ERROR_SUCCESS || pathCount > 256 || modeCount > 512)
            break;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        result = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount,
                                    modes.data(), nullptr);
        if (result == ERROR_INSUFFICIENT_BUFFER)
            continue;
        if (result != ERROR_SUCCESS)
            break;
        for (UINT32 i = 0; i < pathCount; ++i) {
            const auto& path = paths[i];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header = {DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
                             path.sourceInfo.adapterId, path.sourceInfo.id};
            DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
            target.header = {DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME, sizeof(target),
                             path.targetInfo.adapterId, path.targetInfo.id};
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS ||
                DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS ||
                !target.monitorDevicePath[0])
                continue;
            std::wstring alias = source.viewGdiDeviceName;
            if (alias.starts_with(L"\\\\.\\"))
                alias.erase(0, 4);
            for (auto& monitor : monitors) {
                if (normalizeMonitorSelector(monitor.alias) != normalizeMonitorSelector(alias) ||
                    !monitor.id.starts_with(L"gdi:"))
                    continue;
                monitor.id = target.monitorDevicePath;
                if (path.targetInfo.refreshRate.Denominator)
                    monitor.refreshHz = static_cast<double>(path.targetInfo.refreshRate.Numerator) /
                                        path.targetInfo.refreshRate.Denominator;
            }
        }
        return;
    }
    logger.warning("DisplayConfig IDs unavailable; GDI fallback IDs can change after reconnection");
}
} // namespace
std::vector<Monitor> enumerateMonitors(Logger& logger) {
    const ScopedDpiContext physical(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Enumeration result;
    if (!EnumDisplayMonitors(nullptr, nullptr, collectMonitor, reinterpret_cast<LPARAM>(&result)) ||
        result.failed || result.monitors.empty())
        throw std::runtime_error("Display enumeration failed");
    assignPersistentIds(result.monitors, logger);
    std::set<std::wstring> ids;
    std::stable_sort(result.monitors.begin(), result.monitors.end(),
                     [](const Monitor& a, const Monitor& b) {
                         if (a.primary != b.primary)
                             return a.primary;
                         return a.alias < b.alias;
                     });
    for (const auto& monitor : result.monitors) {
        if (!ids.insert(normalizeMonitorSelector(monitor.id)).second)
            throw std::runtime_error("Duplicate monitor identity");
        logger.info("Monitor alias=" + utf8(monitor.alias) + " id=" + utf8(monitor.id) + " rect=" +
                    std::to_string(monitor.bounds.left) + "," + std::to_string(monitor.bounds.top) +
                    "," + std::to_string(monitor.bounds.right) + "," +
                    std::to_string(monitor.bounds.bottom) +
                    " refreshHz=" + std::to_string(monitor.refreshHz) +
                    " primary=" + std::to_string(monitor.primary));
    }
    return result.monitors;
}
} // namespace aetherwall
