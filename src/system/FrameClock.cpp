#include "system/FrameClock.h"

#include <stdexcept>

namespace aetherwall {

FrameClock::FrameClock(int fps, TimePoint now) : next_(now) {
    if (fps < 1 || fps > 1000) {
        throw std::invalid_argument("Invalid frame rate");
    }
    period_ = std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0 / fps));
}

bool FrameClock::due(TimePoint now) const { return now >= next_; }

void FrameClock::frameStarted(TimePoint started) {
    // Cap actual frame starts, without accumulating missed deadlines. Time blocked
    // inside Present counts toward this period rather than adding a second delay.
    next_ = started + period_;
}

void FrameClock::restart(TimePoint now) { next_ = now; }

std::chrono::milliseconds FrameClock::wait(TimePoint now) const {
    if (due(now)) {
        return std::chrono::milliseconds::zero();
    }
    return std::chrono::ceil<std::chrono::milliseconds>(next_ - now);
}

} // namespace aetherwall
