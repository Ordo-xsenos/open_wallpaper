#include "CoreApplication.h"
#include "aetherwall_version.h"
#include "diagnostics/Logger.h"
#include "system/Options.h"
#include "system/WindowsEnvironment.h"
#include "system/WindowsSupport.h"

#include <atomic>
#include <iostream>
#include <vector>

namespace {
std::atomic_bool interrupted{false};

BOOL WINAPI consoleHandler(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        interrupted.store(true, std::memory_order_relaxed);
        return TRUE;
    }
    return FALSE;
}

class ConsoleHandler {
  public:
    ConsoleHandler() {
        // Manager starts the background Core without a console. Registering a
        // console handler in that process can fail before the engine is initialized.
        if (GetConsoleCP() == 0)
            return;
        if (!SetConsoleCtrlHandler(consoleHandler, TRUE)) {
            aetherwall::throwLastError("SetConsoleCtrlHandler");
        }
        registered_ = true;
    }
    ~ConsoleHandler() {
        if (registered_)
            SetConsoleCtrlHandler(consoleHandler, FALSE);
    }

  private:
    bool registered_ = false;
};

class ComApartment {
  public:
    ComApartment() {
        aetherwall::checkHr(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "CoInitializeEx");
    }
    ~ComApartment() { CoUninitialize(); }
};
} // namespace

int wmain(int argumentCount, wchar_t* arguments[]) {
    using namespace aetherwall;
    Options options;
    try {
        std::vector<std::wstring> values;
        for (int index = 1; index < argumentCount; ++index) {
            values.emplace_back(arguments[index]);
        }
        options = parseOptions(values);
    } catch (const std::exception& error) {
        std::cerr << "Argument error: " << error.what() << '\n';
        std::wcerr << usage();
        return 2;
    }
    if (options.help) {
        std::wcout << usage();
        return 0;
    }
    try {
        Logger logger(options.logDirectory, options.debug);
        logger.debug("Core process entered main; pid=" + std::to_string(GetCurrentProcessId()));
        try {
            UniqueHandle runningMutex(
                CreateMutexW(nullptr, FALSE, L"Local\\AetherWall.Core.Running"));
            if (!runningMutex)
                throwLastError("Create Core running marker");
            logger.debug("Initializing COM and console handling");
            const ComApartment com;
            const ConsoleHandler console;
            if (options.service && options.mode == RunMode::Desktop && !options.listMonitors)
                FreeConsole();
            logger.info("AetherWall " AETHERWALL_VERSION "; mode=" +
                        std::string(options.mode == RunMode::Desktop ? "desktop" : "window") +
                        " fps=" + std::to_string(options.fps) +
                        " durationSeconds=" + std::to_string(options.durationSeconds) +
                        " vsync=" + std::to_string(options.vsync));
            logger.debug("Checking Windows environment and DPI awareness");
            initializeWindowsEnvironment(logger, options.mode == RunMode::Desktop);
            CoreApplication application(logger, options);
            logger.debug("Entering Core application");
            const auto result = application.run(interrupted);
            logger.debug("Core shutdown complete; exit=" + std::to_string(result));
            return result;
        } catch (const std::exception& error) {
            // CoreApplication has already destroyed its surface and graphics resources.
            logger.error(std::string("Safe failure: ") + error.what());
            return 1;
        }
    } catch (const std::exception& error) {
        std::cerr << "Initialization failed: " << error.what() << '\n';
        return 1;
    }
}
