#include "system/UserConfiguration.h"
#include "system/WindowsSupport.h"
#include <shellapi.h>
#include <vector>

namespace aetherwall {
std::filesystem::path executableDirectory() {
    std::vector<wchar_t> value(32768);
    const DWORD count = GetModuleFileNameW(nullptr, value.data(), static_cast<DWORD>(value.size()));
    if (!count || count >= value.size())
        throwLastError("GetModuleFileNameW");
    return std::filesystem::path(std::wstring(value.data(), count)).parent_path();
}
std::filesystem::path defaultDataDirectory() {
    const DWORD count = GetEnvironmentVariableW(L"LOCALAPPDATA", nullptr, 0);
    if (!count || count > 32768)
        throw std::runtime_error("LOCALAPPDATA unavailable");
    std::vector<wchar_t> value(count);
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", value.data(), count))
        throwLastError("GetEnvironmentVariableW");
    return std::filesystem::path(value.data()) / L"AetherWall";
}
namespace {
std::wstring quote(std::wstring value) {
    // Windows paths cannot contain quotes; remove trailing separators before quoting.
    while (value.size() > 3 && (value.back() == L'\\' || value.back() == L'/'))
        value.pop_back();
    if (value.find(L'"') != std::wstring::npos)
        throw std::invalid_argument("Quote in configuration path");
    if (!value.empty() && value.back() == L'\\')
        value += L'\\';
    return L"\"" + value + L"\"";
}
} // namespace
void configureAutostart(bool enabled, const std::filesystem::path& dataDirectory) {
    HKEY key = nullptr;
    const LSTATUS opened =
        RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0,
                        nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (opened != ERROR_SUCCESS)
        throw WindowsError("Open current-user autostart", HRESULT_FROM_WIN32(opened));
    LSTATUS result = ERROR_SUCCESS;
    if (enabled) {
        const auto command = quote((executableDirectory() / L"AetherWall.Core.exe").wstring()) +
                             L" --service --data-dir " +
                             quote(std::filesystem::absolute(dataDirectory).wstring());
        result = RegSetValueExW(key, L"AetherWall", 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        result = RegDeleteValueW(key, L"AetherWall");
        if (result == ERROR_FILE_NOT_FOUND)
            result = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    if (result != ERROR_SUCCESS)
        throw WindowsError("Set current-user autostart", HRESULT_FROM_WIN32(result));
}
void openManager() {
    const auto path = executableDirectory() / L"AetherWall.Manager.exe";
    if (!std::filesystem::is_regular_file(path))
        throw std::runtime_error(
            "AetherWall.Manager.exe is missing beside Core; build and deploy Manager first");
    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_FLAG_NO_UI;
    info.lpFile = path.c_str();
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info))
        throwLastError("Open Manager");
}
namespace {
BOOL CALLBACK closeManager(HWND window, LPARAM data) {
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    UniqueHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
    if (!process)
        return TRUE;
    wchar_t name[32768]{};
    DWORD size = 32768;
    if (QueryFullProcessImageNameW(process.get(), 0, name, &size) &&
        _wcsicmp(name, reinterpret_cast<const wchar_t*>(data)) == 0)
        PostMessageW(window, WM_CLOSE, 0, 0);
    return TRUE;
}
} // namespace
void closeManagerWindows() noexcept {
    try {
        const auto path = (executableDirectory() / L"AetherWall.Manager.exe").wstring();
        EnumWindows(closeManager, reinterpret_cast<LPARAM>(path.c_str()));
    } catch (...) {
    }
}
} // namespace aetherwall
