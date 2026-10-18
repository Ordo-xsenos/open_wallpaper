#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace aetherwall {
struct FrameSummary {
    double averageMs = 0;
    double p95Ms = 0;
    std::size_t samples = 0;
};
class FrameMetrics {
  public:
    void add(double milliseconds) {
        if (!std::isfinite(milliseconds) || milliseconds < 0)
            return;
        samples_[next_] = milliseconds;
        next_ = (next_ + 1) % samples_.size();
        count_ = std::min(count_ + 1, samples_.size());
    }
    FrameSummary summary() const {
        if (!count_)
            return {};
        auto sorted = samples_;
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(count_));
        double sum = 0;
        for (std::size_t i = 0; i < count_; ++i)
            sum += sorted[i];
        const auto rank = static_cast<std::size_t>(std::ceil(0.95 * static_cast<double>(count_)));
        return {sum / static_cast<double>(count_), sorted[rank - 1], count_};
    }
    void clear() { next_ = count_ = 0; }

  private:
    std::array<double, 600> samples_{};
    std::size_t next_ = 0;
    std::size_t count_ = 0;
};
} // namespace aetherwall
