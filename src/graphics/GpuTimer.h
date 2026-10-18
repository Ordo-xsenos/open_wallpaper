#pragma once
#include "diagnostics/FrameMetrics.h"
#include <array>
#include <d3d11.h>
#include <wrl/client.h>

namespace aetherwall {
class Logger;
// Four asynchronous slots: never flush or wait for GPU timestamp results.
class GpuTimer {
  public:
    void initialize(ID3D11Device* device, Logger& logger);
    void begin(ID3D11DeviceContext* context);
    void end(ID3D11DeviceContext* context);
    FrameSummary collect(ID3D11DeviceContext* context);
    void reset() noexcept;

  private:
    void poll(ID3D11DeviceContext* context);
    Logger* logger_ = nullptr;
    struct Slot {
        Microsoft::WRL::ComPtr<ID3D11Query> disjoint, first, last;
        bool pending = false;
    };
    std::array<Slot, 4> slots_{};
    FrameMetrics metrics_;
    int active_ = -1;
    bool enabled_ = false;
};
} // namespace aetherwall
