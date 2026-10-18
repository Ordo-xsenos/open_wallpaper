#pragma once
#include <Windows.h>
#include <chrono>
#include <cstdint>

namespace aetherwall {
class Logger;
class ProcessMetrics {
  public:
    void log(Logger& logger);

  private:
    std::chrono::steady_clock::time_point last_{};
    std::uint64_t cpuTicks_ = 0, threadTicks_ = 0;
    std::chrono::steady_clock::time_point threadLast_{};
};
} // namespace aetherwall
