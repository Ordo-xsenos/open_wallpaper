#pragma once

#include <array>
#include <chrono>
#include <cstddef>

namespace aetherwall {
class RecoveryPolicy {
  public:
    using Clock = std::chrono::steady_clock;
    bool beginAttempt(Clock::time_point now) {
        std::size_t alive = 0;
        for (std::size_t i = 0; i < count_; ++i)
            if (now - attempts_[i] < std::chrono::seconds(30))
                attempts_[alive++] = attempts_[i];
        count_ = alive;
        if (failures_ >= 3 || count_ == attempts_.size())
            return false;
        attempts_[count_++] = now;
        return true;
    }
    void completed(bool success) { failures_ = success ? 0 : failures_ + 1; }

  private:
    std::array<Clock::time_point, 6> attempts_{};
    std::size_t count_ = 0;
    unsigned failures_ = 0;
};
} // namespace aetherwall
