#pragma once

#include "system/PausePolicy.h"

#include <filesystem>
#include <span>
#include <string>

namespace aetherwall {

enum class RunMode { Window, Desktop };
enum class SwapChainMode { Auto, Flip, Blt };
enum class GpuPreference { Auto, PowerSaving, HighPerformance };

struct Options {
    RunMode mode = RunMode::Window;
    SwapChainMode swapChain = SwapChainMode::Auto;
    int fps = 30;
    int durationSeconds = 60;
    bool vsync = true;
    bool warp = false;
    bool help = false;
    bool service = false;
    bool debug = false;
    std::filesystem::path dataDirectory;
    bool allMonitors = false;
    bool listMonitors = false;
    GpuPreference gpu = GpuPreference::Auto;
    PauseSettings pause;
    std::map<std::wstring, std::filesystem::path> monitorScenes;
    std::filesystem::path logDirectory;
    std::filesystem::path scenePath;
};

Options parseOptions(std::span<const std::wstring> arguments);
const wchar_t* usage();

} // namespace aetherwall
