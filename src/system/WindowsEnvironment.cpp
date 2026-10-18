#include "system/WindowsEnvironment.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include <cstring>

namespace aetherwall {

void initializeWindowsEnvironment(Logger& logger, bool desktopMode) {
    using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) {
        throwLastError("GetModuleHandleW(ntdll)");
    }
    const auto address = GetProcAddress(ntdll, "RtlGetVersion");
    if (!address) {
        throwLastError("GetProcAddress(RtlGetVersion)");
    }
    RtlGetVersionFunction getVersion = nullptr;
    static_assert(sizeof(getVersion) == sizeof(address));
    std::memcpy(&getVersion, &address, sizeof(getVersion));
    OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (getVersion(&version) != 0) {
        throw std::runtime_error("RtlGetVersion failed");
    }
    logger.info("Windows version=" + std::to_string(version.dwMajorVersion) + "." +
                std::to_string(version.dwMinorVersion) +
                " build=" + std::to_string(version.dwBuildNumber) + " architecture=x64");
    if (desktopMode && (version.dwMajorVersion != 10 || version.dwBuildNumber < 26100)) {
        throw std::runtime_error("Desktop mode requires Windows 11 24H2+ (build 26100+)");
    }
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
        throwLastError("SetProcessDpiAwarenessContext(PMv2)");
    }
    logger.info("DPI awareness=PerMonitorV2 (set before window creation)");
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) {
        throwLastError("ProcessIdToSessionId");
    }
    logger.info("interactive session=" + std::to_string(session) +
                " remote=" + std::to_string(GetSystemMetrics(SM_REMOTESESSION)));
    if (desktopMode && session == 0) {
        throw std::runtime_error("Desktop mode requires an interactive user session");
    }
}

} // namespace aetherwall
