#include "desktop/Win11DesktopHost.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

#include <array>
#include <vector>

namespace aetherwall {
namespace {
std::wstring className(HWND window) {
    std::array<wchar_t, 256> buffer{};
    const int length = GetClassNameW(window, buffer.data(), static_cast<int>(buffer.size()));
    return length > 0 ? std::wstring(buffer.data(), static_cast<std::size_t>(length)) : L"";
}

DWORD processId(HWND window) {
    DWORD process = 0;
    GetWindowThreadProcessId(window, &process);
    return process;
}

BOOL CALLBACK collectWindows(HWND window, LPARAM parameter) {
    auto& windows = *reinterpret_cast<std::vector<HWND>*>(parameter);
    try {
        if (windows.size() >= 4096) {
            SetLastError(ERROR_INSUFFICIENT_BUFFER);
            return FALSE;
        }
        windows.push_back(window);
        return TRUE;
    } catch (...) {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }
}

bool precedesInZOrder(HWND front, HWND back) {
    // A conservative bound prevents loops if topology changes during enumeration.
    HWND window = front;
    for (int index = 0; window && index < 4096; ++index) {
        if (window == back) {
            return true;
        }
        window = GetWindow(window, GW_HWNDNEXT);
    }
    return false;
}

std::string windowDescription(HWND window) {
    RECT bounds{};
    GetWindowRect(window, &bounds);
    std::ostringstream value;
    value << "HWND=" << window << " class=" << utf8(className(window))
          << " pid=" << processId(window) << " parent=" << GetParent(window)
          << " visible=" << IsWindowVisible(window) << " rect=" << bounds.left << "," << bounds.top
          << "," << bounds.right << "," << bounds.bottom << " dpi=" << GetDpiForWindow(window)
          << " awareness="
          << GetAwarenessFromDpiAwarenessContext(GetWindowDpiAwarenessContext(window));
    return value.str();
}
} // namespace

void Windows11DesktopHost::discover() {
    reset();
    const HWND progman = FindWindowW(L"Progman", nullptr);
    const HWND shell = GetShellWindow();
    if (!progman || !shell || processId(progman) != processId(shell)) {
        throw std::runtime_error("A consistent Explorer/Progman desktop was not found");
    }
    shellProcess_ = processId(shell);
    if (!shellProcess_) {
        throw std::runtime_error("Shell process is unavailable");
    }
    logger_.info("Shell discovery: " + windowDescription(progman));

    // Undocumented Shell convention: request a WorkerW behind the icon-bearing window.
    // No pointer payload, no injection, and no repeated unbounded SendMessage calls.
    DWORD_PTR response = 0;
    SetLastError(ERROR_SUCCESS);
    if (!SendMessageTimeoutW(progman, 0x052C, 0, 0,
                             SMTO_ABORTIFHUNG | SMTO_BLOCK | SMTO_ERRORONEXIT, 1000, &response)) {
        throwLastError("SendMessageTimeoutW(Progman, experimental 0x052C)");
    }
    logger_.info("Experimental 0x052C delivered; response=" + std::to_string(response));

    std::vector<HWND> windows;
    SetLastError(ERROR_SUCCESS);
    if (!EnumWindows(collectWindows, reinterpret_cast<LPARAM>(&windows))) {
        throwLastError("EnumWindows");
    }
    for (const HWND window : windows) {
        const auto name = className(window);
        if (name == L"Progman" || name == L"WorkerW") {
            logger_.info("Shell topology: " + windowDescription(window));
            std::vector<HWND> descendants;
            EnumChildWindows(window, collectWindows, reinterpret_cast<LPARAM>(&descendants));
            for (const HWND child : descendants) {
                const auto childClass = className(child);
                if (childClass == L"WorkerW" || childClass == L"SHELLDLL_DefView" ||
                    childClass == L"SysListView32") {
                    logger_.info("Shell descendant: " + windowDescription(child));
                }
            }
            const HWND view = FindWindowExW(window, nullptr, L"SHELLDLL_DefView", nullptr);
            if (view) {
                logger_.info("Icon view: " + windowDescription(view));
            }
            if (view && processId(window) == shellProcess_ && IsWindowVisible(window)) {
                if (iconHost_) {
                    throw std::runtime_error(
                        "Multiple icon hosts found; refusing ambiguous topology");
                }
                iconHost_ = window;
                iconView_ = view;
            }
        }
    }
    if (!iconHost_) {
        throw std::runtime_error("No supported top-level icon host found; safe refusal");
    }
    // Only accept an existing, visible, empty WorkerW behind the icon host.
    // Newer nested Shell topologies are deliberately refused until tested and documented.
    HWND candidate = GetWindow(iconHost_, GW_HWNDNEXT);
    for (int remaining = 4096; candidate && remaining > 0;
         --remaining, candidate = GetWindow(candidate, GW_HWNDNEXT)) {
        if (className(candidate) == L"WorkerW" && processId(candidate) == shellProcess_ &&
            IsWindowVisible(candidate) &&
            !FindWindowExW(candidate, nullptr, L"SHELLDLL_DefView", nullptr) &&
            !GetWindow(candidate, GW_CHILD)) {
            host_ = candidate;
            break;
        }
    }
    if (!host_) {
        throw std::runtime_error(
            "No empty WorkerW behind icons; unsupported topology, safe refusal");
    }
    logger_.info("Selected desktop host (experimental): " + windowDescription(host_));
}

void Windows11DesktopHost::attach(HWND surface) {
    if (!IsWindow(surface) || IsWindowVisible(surface) || !IsWindow(host_) ||
        !IsWindow(iconHost_) || !precedesInZOrder(iconHost_, host_)) {
        throw std::runtime_error("Desktop attachment preconditions failed");
    }
    if (!AreDpiAwarenessContextsEqual(GetWindowDpiAwarenessContext(surface), dpiContext())) {
        throw std::runtime_error("Surface/host DPI contexts differ; refusing cross-DPI SetParent");
    }
    // Styles are assigned by Core before SetParent, as required by Win32.
    SetLastError(ERROR_SUCCESS);
    const HWND oldParent = SetParent(surface, host_);
    if (!oldParent && GetLastError() != ERROR_SUCCESS) {
        throwLastError("SetParent(desktop surface)");
    }
    logger_.info("Attached hidden surface: " + windowDescription(surface));
    if (!valid(surface)) {
        hide(surface);
        throw std::runtime_error("Desktop topology invalid after attachment");
    }
}

void Windows11DesktopHost::position(HWND surface, const PixelRect& monitor) {
    if (!valid(surface) || !validRect(monitor))
        throw std::runtime_error("Invalid desktop placement");
    {
        const ScopedDpiContext physical(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        RECT parent{};
        if (!GetWindowRect(host_, &parent))
            throwLastError("GetWindowRect(WorkerW)");
        if (!covers({static_cast<int>(parent.left), static_cast<int>(parent.top),
                     static_cast<int>(parent.right), static_cast<int>(parent.bottom)},
                    monitor, 0))
            throw std::runtime_error("WorkerW does not cover selected monitor; safe refusal");
    }
    POINT points[2]{{monitor.left, monitor.top}, {monitor.right, monitor.bottom}};
    for (auto& point : points)
        if (!PhysicalToLogicalPointForPerMonitorDPI(host_, &point))
            throwLastError("PhysicalToLogicalPointForPerMonitorDPI");
    const ScopedDpiContext parentDpi(dpiContext());
    SetLastError(ERROR_SUCCESS);
    if (!MapWindowPoints(HWND_DESKTOP, host_, points, 2) && GetLastError() != ERROR_SUCCESS)
        throwLastError("MapWindowPoints(monitor to WorkerW)");
    if (!SetWindowPos(surface, HWND_BOTTOM, points[0].x, points[0].y, points[1].x - points[0].x,
                      points[1].y - points[0].y, SWP_NOACTIVATE | SWP_FRAMECHANGED))
        throwLastError("SetWindowPos(monitor)");
}

DPI_AWARENESS_CONTEXT Windows11DesktopHost::dpiContext() const {
    if (!IsWindow(host_)) {
        throw std::runtime_error("Desktop host is unavailable for DPI context selection");
    }
    const auto context = GetWindowDpiAwarenessContext(host_);
    if (!context) {
        throwLastError("GetWindowDpiAwarenessContext(host)");
    }
    return context;
}

bool Windows11DesktopHost::valid(HWND surface) const {
    if (!IsWindow(surface) || !IsWindow(host_) || !IsWindow(iconHost_) || !IsWindow(iconView_) ||
        GetParent(surface) != host_ || processId(host_) != shellProcess_ ||
        processId(iconHost_) != shellProcess_ || processId(GetShellWindow()) != shellProcess_ ||
        GetParent(iconView_) != iconHost_ || !IsWindowVisible(host_) ||
        !IsWindowVisible(iconHost_) || GetAncestor(host_, GA_ROOT) != host_ ||
        GetAncestor(iconHost_, GA_ROOT) != iconHost_ || !precedesInZOrder(iconHost_, host_)) {
        return false;
    }
    return className(host_) == L"WorkerW" && className(iconView_) == L"SHELLDLL_DefView" &&
           !FindWindowExW(host_, nullptr, L"SHELLDLL_DefView", nullptr);
}

void Windows11DesktopHost::hide(HWND surface) noexcept {
    if (surface && IsWindow(surface)) {
        ShowWindow(surface, SW_HIDE);
    }
}

void Windows11DesktopHost::reset() noexcept {
    host_ = nullptr;
    iconHost_ = nullptr;
    iconView_ = nullptr;
    shellProcess_ = 0;
}

} // namespace aetherwall
