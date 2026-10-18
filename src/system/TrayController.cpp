#include "system/TrayController.h"
#include "system/WindowsSupport.h"

namespace aetherwall {
TrayController::~TrayController() { remove(); }
void TrayController::remove() noexcept {
    if (installed_)
        Shell_NotifyIconW(NIM_DELETE, &icon_);
    installed_ = false;
}
void TrayController::install(HWND owner) {
    icon_.cbSize = sizeof(icon_);
    icon_.hWnd = owner;
    icon_.uID = 1;
    icon_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon_.uCallbackMessage = callbackMessage;
    icon_.hIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101));
    if (!icon_.hIcon)
        icon_.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcscpy_s(icon_.szTip, L"AetherWall");
    restore();
}
void TrayController::restore() {
    installed_ = Shell_NotifyIconW(NIM_ADD, &icon_) != FALSE;
    if (!installed_)
        throwLastError("Shell_NotifyIconW(NIM_ADD)");
    icon_.uVersion = NOTIFYICON_VERSION_4;
    if (!Shell_NotifyIconW(NIM_SETVERSION, &icon_))
        throwLastError("Shell_NotifyIconW(NIM_SETVERSION)");
}
void TrayController::handle(LPARAM event) noexcept {
    const auto code = LOWORD(event);
    if (code == NIN_SELECT || code == NIN_KEYSELECT) {
        PostMessageW(icon_.hWnd, WM_COMMAND, openCommand, 0);
        return;
    }
    if (code != WM_CONTEXTMENU && code != WM_RBUTTONUP)
        return;
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;
    AppendMenuW(menu, MF_STRING, openCommand,
                russian_ ? L"Открыть AetherWall" : L"Open AetherWall");
    AppendMenuW(menu, MF_STRING, pauseCommand,
                paused_ ? (russian_ ? L"Продолжить" : L"Resume")
                        : (russian_ ? L"Пауза" : L"Pause"));
    AppendMenuW(menu, MF_STRING, reloadCommand,
                russian_ ? L"Обновить мониторы" : L"Reload displays");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, stopCommand,
                russian_ ? L"Завершить полностью" : L"Exit completely");
    POINT point{};
    GetCursorPos(&point);
    SetForegroundWindow(icon_.hWnd);
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, 0,
                                        icon_.hWnd, nullptr);
    DestroyMenu(menu);
    PostMessageW(icon_.hWnd, WM_NULL, 0, 0);
    if (command)
        PostMessageW(icon_.hWnd, WM_COMMAND, command, 0);
}
} // namespace aetherwall
