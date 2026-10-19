// A bounded, non-rendering child for Windows process-lifetime tests.
#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <thread>

int wmain(int argc, wchar_t* argv[]) {
    std::filesystem::path directory;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::wstring_view(argv[i]) == L"--log-dir")
            directory = argv[i + 1];
    if (directory.empty())
        return 2;
    if (std::filesystem::exists(directory / "exit"))
        return 23;
    {
        std::ofstream pid(directory / "pid");
        pid << GetCurrentProcessId();
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!std::filesystem::exists(directory / "stop") && std::chrono::steady_clock::now() < end)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    return 0;
}
