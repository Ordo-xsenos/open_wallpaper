#pragma once

#include "control/ControlService.h"
#include "desktop/Win11DesktopHost.h"
#include "diagnostics/FrameMetrics.h"
#include "diagnostics/ProcessMetrics.h"
#include "graphics/D3D11Renderer.h"
#include "ipc/NamedPipeServer.h"
#include "system/FrameClock.h"
#include "system/MonitorService.h"
#include "system/PowerAndForegroundService.h"
#include "system/RecoveryPolicy.h"
#include "system/RenderDemand.h"
#include "system/TrayController.h"
#include "system/WindowsSupport.h"
#include <atomic>
#include <optional>

namespace aetherwall {
class Logger;

// HWNDs, shared immediate contexts, and callbacks stay on the message-loop thread.
class CoreApplication {
  public:
    CoreApplication(Logger& logger, const Options& options);
    ~CoreApplication();
    CoreApplication(const CoreApplication&) = delete;
    CoreApplication& operator=(const CoreApplication&) = delete;
    int run(const std::atomic_bool& interrupted);

  private:
    struct SavedScene {
        std::shared_ptr<const ScenePackage> package = defaultScene();
        std::filesystem::path requestedPath;
        double seconds = 0;
        bool manualPause = false;
        bool loaded = false;
    };
    struct Output {
        Output(Logger& logger, DevicePool& devices, Monitor description, int fps,
               std::shared_ptr<SavedScene> savedState);
        Monitor monitor;
        D3D11Renderer renderer;
        FrameClock frames;
        RenderDemand demand;
        FrameMetrics submission;
        std::shared_ptr<SavedScene> saved;
        std::filesystem::path scenePath;
        HWND window = nullptr;
        std::optional<RECT> dpiRectangle;
        FrameClock::TimePoint tick = FrameClock::Clock::now();
        FrameClock::TimePoint visibilityCheck = tick;
        std::uint64_t presented = 0, attempts = 0, busy = 0;
        std::uint32_t pause = 0;
        bool timelineActive = false;
        bool minimized = false, resize = false, reload = false, occluded = false;
    };
    static LRESULT CALLBACK windowProcedure(HWND window, UINT message, WPARAM wParam,
                                            LPARAM lParam);
    LRESULT onMessage(HWND window, UINT message, WPARAM wParam, LPARAM lParam) noexcept;
    Output* findOutput(HWND window) noexcept;
    void createControlWindow();
    void initializeControl();
    void applySettings(const Json& settings, bool registry);
    Json monitorStatus() const;
    std::function<void()> prepareApply(const std::vector<std::string>& monitors,
                                       const std::filesystem::path& path);
    void pauseOutputs(bool paused, const std::string& selector);
    void createSurfaces();
    void resizeSurface(Output& output);
    void releaseSurfaces() noexcept;
    bool reloadScene(Output& output);
    void logMetrics(double seconds);

    Logger& logger_;
    Options options_;
    Windows11DesktopHost host_;
    DevicePool devices_;
    PowerAndForegroundService power_;
    ProcessMetrics processMetrics_;
    RecoveryPolicy recovery_;
    std::unique_ptr<ControlService> service_;
    std::unique_ptr<NamedPipeServer> ipc_;
    TrayController tray_;
    Json lastMetrics_ = Json::array();
    bool ipcReady_ = false, openManager_ = false, trayPause_ = false, restoreTray_ = false;
    bool assignmentsEnabled_ = false;
    bool globalManualPause_ = false;
    std::optional<FrameClock::TimePoint> stopAfter_;
    std::vector<std::unique_ptr<Output>> outputs_;
    std::map<std::wstring, std::shared_ptr<SavedScene>> saved_;
    std::map<std::filesystem::path, std::weak_ptr<const ScenePackage>> packages_;
    PauseSignals signals_;
    UniqueHandle instanceMutex_;
    HINSTANCE instance_ = nullptr;
    HWND control_ = nullptr;
    UINT taskbarCreated_ = 0;
    bool registered_ = false, sessionNotifications_ = false;
    bool releasing_ = false, stopping_ = false, rebuild_ = false;
    bool locked_ = false, suspended_ = false, policyChanged_ = true;
};
} // namespace aetherwall
