#include "diagnostics/ProcessMetrics.h"
#include "diagnostics/Logger.h"
#include <psapi.h>

namespace aetherwall {
namespace {
std::uint64_t ticks(FILETIME value) {
    return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
}
} // namespace
void ProcessMetrics::log(Logger& logger) {
    FILETIME created{}, exited{}, kernel{}, user{};
    const auto now = std::chrono::steady_clock::now();
    if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) {
        const auto total = ticks(kernel) + ticks(user);
        if (last_ != std::chrono::steady_clock::time_point{}) {
            const double seconds = std::chrono::duration<double>(now - last_).count();
            if (seconds > 0 && total >= cpuTicks_)
                logger.info(
                    "processCpuOneCorePercent=" +
                    std::to_string(static_cast<double>(total - cpuTicks_) / 100000.0 / seconds));
        }
        cpuTicks_ = total;
        last_ = now;
    }
    if (GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) {
        const auto total = ticks(kernel) + ticks(user);
        if (threadLast_ != std::chrono::steady_clock::time_point{}) {
            const double seconds = std::chrono::duration<double>(now - threadLast_).count();
            if (seconds > 0 && total >= threadTicks_)
                logger.info(
                    "renderThreadCpuOneCorePercent=" +
                    std::to_string(static_cast<double>(total - threadTicks_) / 100000.0 / seconds));
        }
        threadTicks_ = total;
        threadLast_ = now;
    }
    PROCESS_MEMORY_COUNTERS_EX memory{};
    memory.cb = sizeof(memory);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)))
        logger.info("workingSetBytes=" + std::to_string(memory.WorkingSetSize) +
                    " privateCommitBytes=" + std::to_string(memory.PrivateUsage));
}
} // namespace aetherwall
