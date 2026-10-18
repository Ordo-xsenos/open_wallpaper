#pragma once

#include <chrono>

namespace aetherwall {

class FrameClock {
  public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    FrameClock(int fps, TimePoint now);
    bool due(TimePoint now) const;
    void frameStarted(TimePoint started);
    void restart(TimePoint now);
    std::chrono::milliseconds wait(TimePoint now) const;

  private:
    Clock::duration period_;
    TimePoint next_;
};

} // namespace aetherwall
