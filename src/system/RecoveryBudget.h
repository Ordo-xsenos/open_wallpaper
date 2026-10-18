#pragma once

#include <cstddef>

namespace aetherwall {

// A lifetime budget prevents repeated successful-but-unstable recoveries from looping forever.
class RecoveryBudget {
  public:
    static constexpr std::size_t limit = 3;
    bool tryConsume() {
        if (used_ == limit) {
            return false;
        }
        ++used_;
        return true;
    }
    std::size_t used() const { return used_; }

  private:
    std::size_t used_ = 0;
};

} // namespace aetherwall
