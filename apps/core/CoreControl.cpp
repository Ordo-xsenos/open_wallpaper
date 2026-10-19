#include "CoreApplication.h"
#include "diagnostics/Logger.h"
#include "scenes/ScenePackage.h"
#include "system/UserConfiguration.h"
#include <algorithm>

namespace aetherwall {
void CoreApplication::initializeControl() {
    RuntimeControl runtime;
    runtime.status = [this] {
        return Json{{"running", true},
                    {"debugLogging", options_.debug},
                    {"renderState", renderState_},
                    {"renderError", renderError_},
                    {"mode", options_.mode == RunMode::Desktop ? "desktop" : "window"},
                    {"outputCount", outputs_.size()},
                    {"dataDirectory", pathText(options_.dataDirectory)},
                    {"logDirectory",
                     pathText(options_.logDirectory.empty() ? defaultDataDirectory() / "logs"
                                                            : options_.logDirectory)}};
    };
    runtime.monitors = [this] { return monitorStatus(); };
    runtime.metrics = [this] { return lastMetrics_; };
    runtime.updateSettings = [this](const Json& settings) { applySettings(settings, true); };
    runtime.prepareApply = [this](const auto& monitors, const auto& path) {
        try {
            return prepareApply(monitors, path);
        } catch (const WindowsError& error) {
            if (error.result() == DXGI_ERROR_DEVICE_REMOVED ||
                error.result() == DXGI_ERROR_DEVICE_RESET)
                rebuild_ = true;
            throw;
        }
    };
    runtime.pause = [this](bool pause, const std::string& selector) {
        pauseOutputs(pause, selector);
    };
    runtime.stop = [this] {
        stopAfter_ = FrameClock::Clock::now() + std::chrono::milliseconds(500);
    };
    runtime.reload = [this] {
        if (renderFailed_) {
            recovery_ = RecoveryPolicy{};
            renderFailed_ = false;
        }
        rebuild_ = true;
    };
    runtime.active = [this](const std::filesystem::path& path) {
        return std::any_of(outputs_.begin(), outputs_.end(),
                           [&](const auto& output) { return output->scenePath == path; });
    };
    if (options_.dataDirectory.empty())
        options_.dataDirectory = defaultDataDirectory();
    options_.dataDirectory = std::filesystem::absolute(options_.dataDirectory);
    service_ = std::make_unique<ControlService>(options_.dataDirectory, std::move(runtime));
    service_->seed(executableDirectory() / "scenes/bundled");
    if (!service_->warning().empty())
        logger_.warning(service_->warning());
    assignmentsEnabled_ = options_.service;
    if (options_.service)
        applySettings(service_->settings(), false);
}
void CoreApplication::applySettings(const Json& settings, bool registry) {
    const auto validated = validateSettings(settings);
    if (registry && validated.at("autostart") != service_->settings().at("autostart"))
        configureAutostart(validated.at("autostart").get<bool>(), options_.dataDirectory);
    const auto gpu = validated.at("gpu").get<std::string>();
    const auto nextGpu = gpu == "auto" ? GpuPreference::Auto
                                       : (gpu == "power-saving" ? GpuPreference::PowerSaving
                                                                : GpuPreference::HighPerformance);
    if (!outputs_.empty() && options_.gpu != nextGpu)
        rebuild_ = true;
    options_.gpu = nextGpu;
    options_.fps = validated.at("fps").get<int>();
    options_.pause.fullscreen = validated.at("pauseFullscreen").get<bool>();
    options_.pause.maximized = validated.at("pauseMaximized").get<bool>();
    options_.pause.battery = validated.at("pauseBattery").get<bool>();
    for (const auto& output : outputs_)
        output->frames = FrameClock(
            std::min(options_.fps, std::max(1, static_cast<int>(output->monitor.refreshHz))),
            FrameClock::Clock::now());
    policyChanged_ = true;
}
Json CoreApplication::monitorStatus() const {
    Json result = Json::array();
    for (const auto& output : outputs_) {
        const auto& m = output->monitor;
        result.push_back({{"id", utf8(m.id)},
                          {"alias", utf8(m.alias)},
                          {"primary", m.primary},
                          {"left", m.bounds.left},
                          {"top", m.bounds.top},
                          {"width", m.bounds.right - m.bounds.left},
                          {"height", m.bounds.bottom - m.bounds.top},
                          {"refreshHz", m.refreshHz},
                          {"dpi", GetDpiForWindow(output->window)},
                          {"manualPause", output->saved->manualPause},
                          {"pauseReasons", output->pause}});
    }
    return result;
}
std::function<void()> CoreApplication::prepareApply(const std::vector<std::string>& monitors,
                                                    const std::filesystem::path& path) {
    if (rebuild_ || outputs_.empty())
        throw std::runtime_error("Displays are recovering; retry when ready");
    std::shared_ptr<const ScenePackage> package;
    if (const auto it = packages_.find(path); it != packages_.end())
        package = it->second.lock();
    if (!package)
        package = loadScenePackage(path);
    packages_[path] = package;
    struct Stage {
        Output* output;
        std::shared_ptr<const TextureMap> textures;
        std::filesystem::path path, requested;
    };
    auto stages = std::make_shared<std::vector<Stage>>();
    for (const auto& id : monitors) {
        const auto it = std::find_if(outputs_.begin(), outputs_.end(), [&](const auto& output) {
            return utf8(output->monitor.id) == id;
        });
        if (it == outputs_.end())
            throw std::invalid_argument("Monitor disconnected");
        stages->push_back({it->get(), (*it)->renderer.prepareScene(package), path, path});
    }
    return [this, stages, package]() noexcept {
        for (auto& stage : *stages) {
            auto& output = *stage.output;
            output.renderer.commitScene(SceneRuntime(package), std::move(stage.textures));
            output.saved->package = package;
            output.saved->seconds = 0;
            output.saved->loaded = true;
            output.scenePath = std::move(stage.path);
            output.saved->requestedPath = std::move(stage.requested);
            output.demand.invalidate();
        }
        assignmentsEnabled_ = true;
    };
}
void CoreApplication::pauseOutputs(bool paused, const std::string& selector) {
    bool found = selector == "*";
    if (found) {
        globalManualPause_ = paused;
        for (const auto& entry : saved_)
            entry.second->manualPause = paused;
    }
    for (const auto& output : outputs_)
        if (selector == "*" || utf8(output->monitor.id) == selector) {
            output->saved->manualPause = paused;
            found = true;
        }
    if (!found)
        throw std::invalid_argument("Monitor is disconnected or unknown");
    policyChanged_ = true;
}
} // namespace aetherwall
