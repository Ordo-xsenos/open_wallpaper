#pragma once
#include "system/MonitorLayout.h"
#include <Windows.h>
#include <vector>

namespace aetherwall {
class Logger;
struct Monitor : MonitorDescription {
    HMONITOR handle = nullptr;
};
std::vector<Monitor> enumerateMonitors(Logger& logger);
} // namespace aetherwall
