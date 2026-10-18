#pragma once

namespace aetherwall {
class RenderDemand {
  public:
    bool needsFrame(bool animated) const { return animated || dirty_; }
    void invalidate() { dirty_ = true; }
    void presented() { dirty_ = false; }

  private:
    bool dirty_ = true;
};
} // namespace aetherwall
