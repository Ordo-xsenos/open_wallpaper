#include "system/Options.h"

#include <limits>
#include <set>
#include <stdexcept>

namespace aetherwall {
namespace {
int parseInteger(const std::wstring& value) {
    if (value.empty()) {
        throw std::invalid_argument("Expected an unsigned decimal integer");
    }
    int result = 0;
    for (wchar_t character : value) {
        if (character < L'0' || character > L'9') {
            throw std::invalid_argument("Expected an unsigned decimal integer");
        }
        const int digit = static_cast<int>(character - L'0');
        if (result > (std::numeric_limits<int>::max() - digit) / 10) {
            throw std::invalid_argument("Integer is too large");
        }
        result = result * 10 + digit;
    }
    return result;
}
} // namespace

Options parseOptions(std::span<const std::wstring> arguments) {
    Options options;
    std::set<std::wstring> seen;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto& name = arguments[index];
        if (name != L"--monitor-scene" && !seen.insert(name).second) {
            throw std::invalid_argument("Duplicate option");
        }
        if (name == L"--service") {
            options.service = true;
            continue;
        }
        if (name == L"--help") {
            options.help = true;
            continue;
        }
        if (name == L"--all-monitors") {
            options.allMonitors = true;
            continue;
        }
        if (name == L"--list-monitors") {
            options.listMonitors = true;
            continue;
        }
        if (name == L"--monitor-scene") {
            if (index + 2 >= arguments.size())
                throw std::invalid_argument("Expected MONITOR and SCENE after --monitor-scene");
            const auto selector = normalizeMonitorSelector(arguments[++index]);
            const auto& path = arguments[++index];
            if (path.empty() || path.starts_with(L"--") || options.monitorScenes.size() >= 16 ||
                !options.monitorScenes.emplace(selector, path).second)
                throw std::invalid_argument(
                    "Duplicate, excessive or invalid monitor scene assignment");
            continue;
        }
        if (name == L"--no-vsync") {
            options.vsync = false;
            continue;
        }
        if (name == L"--warp") {
            options.warp = true;
            continue;
        }
        if (name != L"--mode" && name != L"--fps" && name != L"--duration" &&
            name != L"--swap-chain" && name != L"--log-dir" && name != L"--scene" &&
            name != L"--data-dir" && name != L"--gpu" && name != L"--preset" &&
            name != L"--pause-fullscreen" && name != L"--pause-maximized" &&
            name != L"--pause-battery") {
            throw std::invalid_argument("Unknown option; see --help");
        }
        if (++index == arguments.size()) {
            throw std::invalid_argument("Missing option value");
        }
        const auto& value = arguments[index];
        if (name.starts_with(L"--pause-")) {
            if (value != L"on" && value != L"off")
                throw std::invalid_argument("Pause policy must be on or off");
            const bool enabled = value == L"on";
            if (name == L"--pause-fullscreen")
                options.pause.fullscreen = enabled;
            if (name == L"--pause-maximized")
                options.pause.maximized = enabled;
            if (name == L"--pause-battery")
                options.pause.battery = enabled;
        } else if (name == L"--gpu") {
            if (value == L"auto")
                options.gpu = GpuPreference::Auto;
            else if (value == L"power-saving")
                options.gpu = GpuPreference::PowerSaving;
            else if (value == L"high-performance")
                options.gpu = GpuPreference::HighPerformance;
            else
                throw std::invalid_argument("GPU must be auto, power-saving or high-performance");
        } else if (name == L"--preset") {
            if (value == L"eco")
                options.fps = 15;
            else if (value == L"balanced")
                options.fps = 30;
            else if (value == L"smooth")
                options.fps = 60;
            else
                throw std::invalid_argument("Preset must be eco, balanced or smooth");
        } else if (name == L"--mode") {
            if (value == L"window") {
                options.mode = RunMode::Window;
            } else if (value == L"desktop") {
                options.mode = RunMode::Desktop;
            } else {
                throw std::invalid_argument("Mode must be window or desktop");
            }
        } else if (name == L"--fps") {
            options.fps = parseInteger(value);
            if (options.fps != 15 && options.fps != 30 && options.fps != 60) {
                throw std::invalid_argument("FPS must be 15, 30, or 60");
            }
        } else if (name == L"--duration") {
            options.durationSeconds = parseInteger(value);
            if (options.durationSeconds > 86400) {
                throw std::invalid_argument("Duration must be 0..86400 seconds (0 is unlimited)");
            }
        } else if (name == L"--swap-chain") {
            if (value == L"auto") {
                options.swapChain = SwapChainMode::Auto;
            } else if (value == L"flip") {
                options.swapChain = SwapChainMode::Flip;
            } else if (value == L"blt") {
                options.swapChain = SwapChainMode::Blt;
            } else {
                throw std::invalid_argument("Swap chain must be auto, flip, or blt");
            }
        } else {
            if (value.empty() || value.starts_with(L"--")) {
                throw std::invalid_argument("Expected a directory or file path");
            }
            if (name == L"--data-dir")
                options.dataDirectory = value;
            else if (name == L"--scene")
                options.scenePath = value;
            else
                options.logDirectory = value;
        }
    }
    if (options.service) {
        if (!seen.contains(L"--mode"))
            options.mode = RunMode::Desktop;
        if (!seen.contains(L"--duration"))
            options.durationSeconds = 0;
    }
    if (options.mode == RunMode::Desktop && options.warp) {
        throw std::invalid_argument("WARP is only allowed in diagnostic window mode");
    }
    if (seen.contains(L"--fps") && seen.contains(L"--preset"))
        throw std::invalid_argument("Choose either --fps or --preset");
    if (!options.monitorScenes.empty() && options.mode == RunMode::Window && !options.allMonitors)
        throw std::invalid_argument("Use --all-monitors with window-mode monitor assignments");
    return options;
}

const wchar_t* usage() {
    return L"AetherWall.Core [--mode window|desktop] [--fps 15|30|60]\n"
           L"  [--duration 0..86400] [--swap-chain auto|flip|blt] [--no-vsync]\n"
           L"  [--warp] [--log-dir PATH] [--scene DIRECTORY_OR_AWSCENE] [--help]\n"
           L"  [--service] [--data-dir PATH]\n"
           L"  [--all-monitors] [--list-monitors] [--monitor-scene MONITOR PATH]...\n"
           L"  [--gpu auto|power-saving|high-performance] [--preset eco|balanced|smooth]\n"
           L"  [--pause-fullscreen on|off] [--pause-maximized on|off] [--pause-battery on|off]\n"
           L"Desktop uses all connected monitors; window mode defaults to the primary.\n"
           L"Space in a diagnostic window toggles pause on that output.\n"
           L"F5 in window mode reloads --scene; invalid reload keeps the current scene.\n"
           L"Defaults: window, 30 FPS, 60 seconds, auto swap chain, VSync.\n"
           L"Desktop is experimental; validate window mode first.\n"
           L"Duration 0 runs until Ctrl+C; WARP is diagnostic window mode only.\n";
}
} // namespace aetherwall
