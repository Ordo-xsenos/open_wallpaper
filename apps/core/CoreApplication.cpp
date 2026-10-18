#include "CoreApplication.h"
#include "diagnostics/Logger.h"
#include "scenes/ScenePackage.h"
#include "system/UserConfiguration.h"
#include <WtsApi32.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <set>

namespace aetherwall {
namespace {
constexpr wchar_t windowClass[] = L"AetherWall.Core.Surface";
using Clock = FrameClock::Clock;
using namespace std::chrono_literals;
void setWindowStyle(HWND window, LONG_PTR style) {
    SetLastError(ERROR_SUCCESS);
    if (!SetWindowLongPtrW(window, GWL_STYLE, style) && GetLastError() != ERROR_SUCCESS)
        throwLastError("SetWindowLongPtrW(style)");
}
int outputFps(int requested, double refresh) {
    if (!std::isfinite(refresh) || refresh < 1 || refresh > 1000)
        return requested;
    return std::min(requested, std::max(1, static_cast<int>(std::lround(refresh))));
}
} // namespace
CoreApplication::Output::Output(Logger& logger, DevicePool& devices, Monitor description, int fps,
                                std::shared_ptr<SavedScene> savedState)
    : monitor(std::move(description)), renderer(logger, devices, monitor.handle),
      frames(outputFps(fps, monitor.refreshHz), Clock::now()), saved(std::move(savedState)) {}
CoreApplication::CoreApplication(Logger& logger, const Options& options)
    : logger_(logger), options_(options), host_(logger), devices_(logger), power_(logger) {}
CoreApplication::~CoreApplication() {
    ipc_.reset();
    tray_.remove();
    releaseSurfaces();
    power_.stop();
    if (sessionNotifications_)
        WTSUnRegisterSessionNotification(control_);
    if (control_ && IsWindow(control_))
        DestroyWindow(control_);
    if (registered_)
        UnregisterClassW(windowClass, instance_);
}
void CoreApplication::createControlWindow() {
    instanceMutex_.reset(CreateMutexW(nullptr, FALSE, L"Local\\AetherWall.Core"));
    const DWORD mutexError = GetLastError();
    if (!instanceMutex_)
        throwLastError("CreateMutexW");
    if (mutexError == ERROR_ALREADY_EXISTS)
        throw std::runtime_error("Another AetherWall instance is running in this session");
    instance_ = GetModuleHandleW(nullptr);
    if (!instance_)
        throwLastError("GetModuleHandleW");
    WNDCLASSEXW description{};
    description.cbSize = sizeof(description);
    description.lpfnWndProc = windowProcedure;
    description.hInstance = instance_;
    description.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    description.lpszClassName = windowClass;
    if (!RegisterClassExW(&description))
        throwLastError("RegisterClassExW");
    registered_ = true;
    taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
    if (!taskbarCreated_)
        throwLastError("RegisterWindowMessageW(TaskbarCreated)");
    control_ =
        CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, windowClass, L"AetherWall control",
                        WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance_, this);
    if (!control_)
        throwLastError("CreateWindowExW(control)");
    if (!WTSRegisterSessionNotification(control_, NOTIFY_FOR_THIS_SESSION))
        throwLastError("WTSRegisterSessionNotification");
    sessionNotifications_ = true;
    power_.start(control_);
}
void CoreApplication::createSurfaces() {
    const bool desktop = options_.mode == RunMode::Desktop;
    auto monitors = enumerateMonitors(logger_);
    if (!desktop && !options_.allMonitors)
        monitors.resize(1);
    std::set<std::wstring> connected;
    for (const auto& monitor : monitors)
        connected.insert(normalizeMonitorSelector(monitor.id));
    // Retain disconnected assignments for recovery, bounded to sixteen CPU snapshots.
    for (auto it = saved_.begin(); it != saved_.end() && saved_.size() + monitors.size() > 16;) {
        if (!connected.contains(it->first))
            it = saved_.erase(it);
        else
            ++it;
    }
    for (const auto& [selector, path] : options_.monitorScenes) {
        (void)path;
        const bool found =
            std::any_of(monitors.begin(), monitors.end(), [&](const Monitor& monitor) {
                return selector == normalizeMonitorSelector(monitor.id) ||
                       selector == normalizeMonitorSelector(monitor.alias);
            });
        if (!found)
            logger_.warning("Scene selector is disconnected or not selected: " + utf8(selector));
    }
    if (desktop)
        host_.discover();
    for (auto& monitor : monitors) {
        const auto key = normalizeMonitorSelector(monitor.id);
        auto& saved = saved_[key];
        if (!saved) {
            saved = std::make_shared<SavedScene>();
            saved->manualPause = globalManualPause_;
        }
        auto next =
            std::make_unique<Output>(logger_, devices_, std::move(monitor), options_.fps, saved);
        next->scenePath = assignedScene(next->monitor, options_.monitorScenes, options_.scenePath);
        if (assignmentsEnabled_ && service_) {
            const auto assignments = service_->library().assignments();
            const auto identity = utf8(next->monitor.id);
            if (assignments.contains(identity))
                next->scenePath =
                    service_->library().scenePath(assignments.at(identity).get<std::string>());
        }
        if (saved->requestedPath != next->scenePath) {
            saved->requestedPath = next->scenePath;
            saved->loaded = false;
        }
        outputs_.push_back(std::move(next));
        auto& output = *outputs_.back();
        const ScopedDpiContext dpi(desktop ? host_.dpiContext()
                                           : DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        const auto& work = output.monitor.work;
        const std::wstring title =
            L"AetherWall M2 - " + output.monitor.alias + L" (F5 reload, Space pause)";
        output.window = CreateWindowExW(
            desktop ? WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE : 0, windowClass, title.c_str(),
            desktop ? WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN : WS_OVERLAPPEDWINDOW,
            work.left + 24, work.top + 24, std::min(960, work.right - work.left),
            std::min(540, work.bottom - work.top), nullptr, nullptr, instance_, this);
        if (!output.window)
            throwLastError("CreateWindowExW(output)");
        if (desktop) {
            setWindowStyle(output.window, WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
            host_.attach(output.window);
            host_.position(output.window, output.monitor.bounds);
        }
        logger_.info("Output alias=" + utf8(output.monitor.alias) + " HWND=" +
                     std::to_string(reinterpret_cast<std::uintptr_t>(output.window)) + " fpsCap=" +
                     std::to_string(outputFps(options_.fps, output.monitor.refreshHz)) +
                     " surfaceDpi=" + std::to_string(GetDpiForWindow(output.window)));
        output.renderer.initialize(output.window, options_);
        output.renderer.setScene(saved->package);
        if (!saved->loaded) {
            reloadScene(output);
            saved->loaded = true;
        }
        output.demand.invalidate();
        output.frames.restart(Clock::now());
    }
    // Validate the entire set before showing any surface.
    if (desktop)
        for (const auto& output : outputs_)
            if (!host_.valid(output->window))
                throw std::runtime_error(
                    "Desktop host changed during multi-monitor initialization");
    for (const auto& output : outputs_)
        ShowWindow(output->window, desktop ? SW_SHOWNOACTIVATE : SW_SHOW);
    rebuild_ = false;
    policyChanged_ = true;
    logger_.info("Output set initialized: " + std::to_string(outputs_.size()) +
                 "; Windows visual verification required");
}
void CoreApplication::resizeSurface(Output& output) {
    if (output.dpiRectangle && options_.mode == RunMode::Window) {
        const RECT rect = *output.dpiRectangle;
        output.dpiRectangle.reset();
        if (!SetWindowPos(output.window, nullptr, rect.left, rect.top, rect.right - rect.left,
                          rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE))
            throwLastError("SetWindowPos(DPI)");
    }
    RECT client{};
    if (!GetClientRect(output.window, &client))
        throwLastError("GetClientRect(resize)");
    output.renderer.resize(static_cast<UINT>(client.right), static_cast<UINT>(client.bottom));
    output.resize = false;
    output.demand.invalidate();
}
bool CoreApplication::reloadScene(Output& output) {
    if (output.scenePath.empty())
        return false;
    try {
        const auto key = std::filesystem::absolute(output.scenePath).lexically_normal();
        std::shared_ptr<const ScenePackage> package;
        // Startup/reconnection shares snapshots; F5 always re-reads this output's scene.
        if (!output.reload) {
            if (const auto it = packages_.find(key); it != packages_.end())
                package = it->second.lock();
        }
        if (!package)
            package = loadScenePackage(key);
        output.renderer.setScene(package);
        output.saved->package = package;
        output.saved->seconds = 0;
        output.saved->loaded = true;
        for (auto it = packages_.begin(); it != packages_.end();)
            if (it->second.expired())
                it = packages_.erase(it);
            else
                ++it;
        packages_[key] = std::move(package);
        output.demand.invalidate();
        logger_.info("Scene loaded on " + utf8(output.monitor.alias) + ": " + utf8(key.wstring()));
        return true;
    } catch (const WindowsError& error) {
        if (error.result() == DXGI_ERROR_DEVICE_REMOVED ||
            error.result() == DXGI_ERROR_DEVICE_RESET)
            throw;
        logger_.error(std::string("Scene rejected; previous scene retained: ") + error.what());
    } catch (const std::exception& error) {
        logger_.error(std::string("Scene rejected; previous scene retained: ") + error.what());
    }
    return false;
}
void CoreApplication::releaseSurfaces() noexcept {
    releasing_ = true;
    for (const auto& output : outputs_)
        host_.hide(output->window);
    for (const auto& output : outputs_) {
        output->renderer.shutdown();
        if (output->window && IsWindow(output->window))
            DestroyWindow(output->window);
    }
    outputs_.clear();
    devices_.clear();
    host_.reset();
    releasing_ = false;
}
void CoreApplication::logMetrics(double seconds) {
    processMetrics_.log(logger_);
    lastMetrics_ = Json::array();
    for (const auto& output : outputs_) {
        const auto cpu = output->submission.summary();
        const auto gpu = output->renderer.gpuTiming();
        logger_.info("Output=" + utf8(output->monitor.alias) + " intervalPresented=" +
                     std::to_string(output->presented) + " wallTimePresentRate=" +
                     std::to_string(static_cast<double>(output->presented) / seconds) +
                     " totalDrawPresents=" + std::to_string(output->attempts) +
                     " totalBusyPresents=" + std::to_string(output->busy) +
                     " submissionAvgMs=" + std::to_string(cpu.averageMs) +
                     " submissionP95Ms=" + std::to_string(cpu.p95Ms) +
                     " submissionSamples=" + std::to_string(cpu.samples) + " gpuDrawAvgMs=" +
                     std::to_string(gpu.averageMs) + " gpuDrawP95Ms=" + std::to_string(gpu.p95Ms) +
                     " gpuSamples=" + std::to_string(gpu.samples) +
                     " pauseReasons=" + std::to_string(output->pause) +
                     " occluded=" + std::to_string(output->occluded));
        lastMetrics_.push_back({{"monitorId", utf8(output->monitor.id)},
                                {"alias", utf8(output->monitor.alias)},
                                {"presentRate", static_cast<double>(output->presented) / seconds},
                                {"submissionAvgMs", cpu.averageMs},
                                {"submissionP95Ms", cpu.p95Ms},
                                {"gpuDrawAvgMs", gpu.averageMs},
                                {"gpuSamples", gpu.samples},
                                {"pauseReasons", output->pause}});
        output->presented = 0;
        output->submission.clear();
    }
}
int CoreApplication::run(const std::atomic_bool& interrupted) {
    if (options_.listMonitors) {
        for (const auto& monitor : enumerateMonitors(logger_))
            std::cout << utf8(monitor.alias) << "\t" << utf8(monitor.id) << '\n';
        return 0;
    }
    createControlWindow();
    initializeControl();
    createSurfaces();
    ipc_ = std::make_unique<NamedPipeServer>(control_);
    tray_.install(control_);
    const auto started = Clock::now();
    auto metricStarted = started, recoveryAt = started, policyAt = started;
    bool recovering = false;
    processMetrics_.log(logger_);
    while (!stopping_ && !interrupted.load(std::memory_order_relaxed)) {
        MSG message{};
        for (int count = 0; count < 256 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
             ++count) {
            if (message.message == WM_QUIT) {
                stopping_ = true;
                break;
            }
            if (!message.hwnd && message.message == PowerAndForegroundService::changedMessage) {
                policyChanged_ = true;
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (ipcReady_) {
            ipcReady_ = false;
            ipc_->process([this](std::string_view request) { return service_->handle(request); });
        }
        if (restoreTray_) {
            restoreTray_ = false;
            tray_.restore();
        }
        if (openManager_) {
            openManager_ = false;
            try {
                openManager();
            } catch (const std::exception& error) {
                logger_.error(error.what());
            }
        }
        if (trayPause_) {
            trayPause_ = false;
            const bool all = std::all_of(outputs_.begin(), outputs_.end(), [](const auto& output) {
                return output->saved->manualPause;
            });
            if (!outputs_.empty())
                pauseOutputs(!all, "*");
        }
        tray_.setState(!outputs_.empty() && std::all_of(outputs_.begin(), outputs_.end(),
                                                        [](const auto& output) {
                                                            return output->saved->manualPause;
                                                        }),
                       service_->settings().at("language") == "ru");
        auto now = Clock::now();
        if (stopAfter_ && now >= *stopAfter_)
            stopping_ = true;
        if (stopping_ || interrupted.load(std::memory_order_relaxed) ||
            (options_.durationSeconds > 0 &&
             now - started >= std::chrono::seconds(options_.durationSeconds)))
            break;
        const bool invalid = std::any_of(outputs_.begin(), outputs_.end(), [&](const auto& output) {
            return !output->window ||
                   (options_.mode == RunMode::Desktop && !host_.valid(output->window));
        });
        if (!recovering && (rebuild_ || invalid || outputs_.empty())) {
            logger_.warning(
                "Output topology/device changed; hiding all surfaces and releasing GPU resources");
            releaseSurfaces();
            recovering = true;
            recoveryAt = now + 1s;
        }
        if (recovering && !locked_ && !suspended_ && now >= recoveryAt) {
            if (!recovery_.beginAttempt(now))
                throw std::runtime_error(
                    "Recovery stopped: three consecutive failures or six rebuilds in 30 seconds");
            try {
                createSurfaces();
                recovery_.completed(true);
                recovering = false;
            } catch (const std::exception& error) {
                releaseSurfaces();
                recovery_.completed(false);
                logger_.error(std::string("Output recovery failed: ") + error.what());
                recoveryAt = Clock::now() + 2s;
            }
        }
        now = Clock::now();
        if (policyChanged_ || now >= policyAt) {
            signals_ = power_.snapshot(locked_, suspended_);
            policyChanged_ = false;
            policyAt = now + 250ms;
        }
        try {
            for (const auto& item : outputs_) {
                auto& output = *item;
                auto local = signals_;
                local.manual = output.saved->manualPause;
                local.minimized = output.minimized;
                const auto reasons = pauseReasons(options_.pause, local, output.monitor.bounds);
                if (reasons != output.pause) {
                    logger_.info("Pause transition output=" + utf8(output.monitor.alias) +
                                 " reasons=" + std::to_string(reasons));
                    output.pause = reasons;
                    output.frames.restart(now);
                    output.demand.invalidate();
                }
                const bool active = reasons == 0 && !output.occluded && !recovering;
                if (active && output.timelineActive)
                    output.saved->seconds +=
                        std::chrono::duration<double>(now - output.tick).count();
                output.timelineActive = active;
                output.tick = now;
                if (reasons)
                    continue;
                if (output.reload) {
                    reloadScene(output);
                    output.reload = false;
                }
                if (output.resize)
                    resizeSurface(output);
                if (output.occluded && now >= output.visibilityCheck) {
                    output.occluded = !output.renderer.visible();
                    output.visibilityCheck = now + 250ms;
                    if (!output.occluded) {
                        output.frames.restart(now);
                        output.demand.invalidate();
                    }
                }
                now = Clock::now();
                if (output.occluded || !output.demand.needsFrame(output.renderer.animated()) ||
                    !output.frames.due(now))
                    continue;
                output.frames.frameStarted(now);
                ++output.attempts;
                const auto result = output.renderer.render(output.saved->seconds, options_.vsync);
                output.submission.add(
                    std::chrono::duration<double, std::milli>(Clock::now() - now).count());
                if (result == PresentResult::Presented) {
                    ++output.presented;
                    output.demand.presented();
                } else if (result == PresentResult::Busy)
                    ++output.busy;
                else {
                    output.occluded = true;
                    output.visibilityCheck = Clock::now() + 250ms;
                }
            }
            now = Clock::now();
            if (!recovering && now - metricStarted >= 5s) {
                logMetrics(std::chrono::duration<double>(now - metricStarted).count());
                metricStarted = now;
            }
        } catch (const WindowsError& error) {
            if (error.result() != DXGI_ERROR_DEVICE_REMOVED &&
                error.result() != DXGI_ERROR_DEVICE_RESET)
                throw;
            logger_.error(std::string("GPU reset; rebuilding output set: ") + error.what());
            rebuild_ = true;
            for (const auto& output : outputs_)
                host_.hide(output->window);
        }
        now = Clock::now();
        auto delay = 250ms;
        for (const auto& output : outputs_)
            if (!rebuild_ && !output->pause && !output->occluded &&
                output->demand.needsFrame(output->renderer.animated()))
                delay = std::min(delay, output->frames.wait(now));
        if (options_.durationSeconds > 0) {
            const auto remaining = std::chrono::ceil<std::chrono::milliseconds>(
                started + std::chrono::seconds(options_.durationSeconds) - now);
            delay = std::min(delay, std::max(0ms, remaining));
        }
        if (MsgWaitForMultipleObjectsEx(0, nullptr, static_cast<DWORD>(delay.count()), QS_ALLINPUT,
                                        MWMO_INPUTAVAILABLE) == WAIT_FAILED)
            throwLastError("MsgWaitForMultipleObjectsEx");
    }
    closeManagerWindows();
    ipc_.reset();
    releaseSurfaces();
    logger_.info("Stopped; all owned surfaces and GPU resources released");
    return 0;
}
CoreApplication::Output* CoreApplication::findOutput(HWND window) noexcept {
    for (const auto& output : outputs_)
        if (output->window == window)
            return output.get();
    return nullptr;
}
LRESULT CALLBACK CoreApplication::windowProcedure(HWND window, UINT message, WPARAM wParam,
                                                  LPARAM lParam) {
    auto* application =
        reinterpret_cast<CoreApplication*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        application = static_cast<CoreApplication*>(
            reinterpret_cast<const CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetLastError(ERROR_SUCCESS);
        if (!SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(application)) &&
            GetLastError() != ERROR_SUCCESS)
            return FALSE;
    }
    if (!application)
        return DefWindowProcW(window, message, wParam, lParam);
    const LRESULT result = application->onMessage(window, message, wParam, lParam);
    if (message == WM_NCDESTROY)
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return result;
}
LRESULT CoreApplication::onMessage(HWND window, UINT message, WPARAM wParam,
                                   LPARAM lParam) noexcept {
    // Callback only updates state; allocations, logging and GPU work happen in run().
    auto* output = findOutput(window);
    if (taskbarCreated_ && message == taskbarCreated_) {
        if (options_.mode == RunMode::Desktop)
            rebuild_ = true;
        restoreTray_ = true;
        return 0;
    }
    if (message == NamedPipeServer::readyMessage) {
        ipcReady_ = true;
        return 0;
    }
    if (message == TrayController::callbackMessage) {
        tray_.handle(lParam);
        return 0;
    }
    switch (message) {
    case WM_COMMAND:
        if (LOWORD(wParam) == TrayController::openCommand)
            openManager_ = true;
        if (LOWORD(wParam) == TrayController::pauseCommand)
            trayPause_ = true;
        if (LOWORD(wParam) == TrayController::reloadCommand)
            rebuild_ = true;
        if (LOWORD(wParam) == TrayController::stopCommand) {
            stopAfter_ = Clock::now() + 500ms;
        }
        return 0;
    case WM_KEYDOWN:
        if (output && wParam == VK_F5)
            output->reload = true;
        if (output && wParam == VK_SPACE && !(lParam & (1LL << 30))) {
            output->saved->manualPause = !output->saved->manualPause;
            policyChanged_ = true;
        }
        return 0;
    case WM_CLOSE:
        stopping_ = true;
        return 0;
    case WM_DESTROY:
        if (output) {
            output->window = nullptr;
            if (!releasing_)
                rebuild_ = true;
        }
        return 0;
    case WM_SIZE:
        if (output) {
            output->minimized = wParam == SIZE_MINIMIZED;
            output->resize = true;
        }
        return 0;
    case WM_DPICHANGED:
        if (window == control_)
            rebuild_ = true;
        if (output) {
            if (options_.mode == RunMode::Desktop)
                rebuild_ = true;
            else {
                output->dpiRectangle = *reinterpret_cast<const RECT*>(lParam);
                output->resize = true;
            }
        }
        return 0;
    case WM_DISPLAYCHANGE:
        rebuild_ = true;
        return 0;
    case WM_SETTINGCHANGE:
        if (wParam == SPI_SETWORKAREA)
            rebuild_ = true;
        return 0;
    case WM_WTSSESSION_CHANGE:
        if (wParam == WTS_SESSION_LOCK)
            locked_ = true;
        else if (wParam == WTS_SESSION_UNLOCK) {
            locked_ = false;
            if (options_.mode == RunMode::Desktop)
                rebuild_ = true;
        }
        policyChanged_ = true;
        return 0;
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND)
            suspended_ = true;
        else if (wParam == PBT_APMRESUMEAUTOMATIC || wParam == PBT_APMRESUMESUSPEND) {
            suspended_ = false;
            rebuild_ = true;
        } else if (wParam == PBT_POWERSETTINGCHANGE && lParam) {
            const auto* setting = reinterpret_cast<const POWERBROADCAST_SETTING*>(lParam);
            if (setting->PowerSetting == GUID_CONSOLE_DISPLAY_STATE &&
                setting->DataLength == sizeof(DWORD)) {
                DWORD state = 0;
                std::memcpy(&state, setting->Data, sizeof(state));
                power_.displayState(state);
            }
        }
        policyChanged_ = true;
        return TRUE;
    case WM_QUERYENDSESSION:
        return TRUE;
    case WM_ENDSESSION:
        if (wParam)
            stopping_ = true;
        return 0;
    case WM_MOUSEACTIVATE:
        if (options_.mode == RunMode::Desktop)
            return MA_NOACTIVATE;
        break;
    case WM_NCHITTEST:
        if (options_.mode == RunMode::Desktop)
            return HTTRANSPARENT;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        if (output)
            output->demand.invalidate();
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    default:
        break;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
} // namespace aetherwall
