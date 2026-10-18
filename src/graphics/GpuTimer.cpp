#include "graphics/GpuTimer.h"
#include "diagnostics/Logger.h"
#include "system/WindowsSupport.h"

namespace aetherwall {
void GpuTimer::initialize(ID3D11Device* device, Logger& logger) {
    reset();
    logger_ = &logger;
    for (auto& slot : slots_) {
        D3D11_QUERY_DESC description{D3D11_QUERY_TIMESTAMP_DISJOINT, 0};
        HRESULT result = device->CreateQuery(&description, slot.disjoint.GetAddressOf());
        description.Query = D3D11_QUERY_TIMESTAMP;
        if (SUCCEEDED(result))
            result = device->CreateQuery(&description, slot.first.GetAddressOf());
        if (SUCCEEDED(result))
            result = device->CreateQuery(&description, slot.last.GetAddressOf());
        if (FAILED(result)) {
            reset();
            if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
                checkHr(result, "CreateQuery");
            logger.warning("GPU timestamps unavailable: " +
                           hexCode(static_cast<unsigned long>(result)));
            return;
        }
    }
    enabled_ = true;
}
void GpuTimer::begin(ID3D11DeviceContext* context) {
    if (!enabled_)
        return;
    poll(context);
    if (!enabled_)
        return;
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        auto& slot = slots_[i];
        if (slot.pending)
            continue;
        active_ = static_cast<int>(i);
        context->Begin(slot.disjoint.Get());
        context->End(slot.first.Get());
        return;
    }
}
void GpuTimer::end(ID3D11DeviceContext* context) {
    if (active_ < 0)
        return;
    auto& slot = slots_[static_cast<std::size_t>(active_)];
    context->End(slot.last.Get());
    context->End(slot.disjoint.Get());
    slot.pending = true;
    active_ = -1;
}
FrameSummary GpuTimer::collect(ID3D11DeviceContext* context) {
    poll(context);
    return metrics_.summary();
}
void GpuTimer::poll(ID3D11DeviceContext* context) {
    if (!context || !enabled_)
        return;
    for (auto& slot : slots_) {
        if (!slot.pending)
            continue;
        D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint{};
        UINT64 first = 0, last = 0;
        const auto read = [this, context](ID3D11Query* query, void* data, UINT bytes) {
            const HRESULT result =
                context->GetData(query, data, bytes, D3D11_ASYNC_GETDATA_DONOTFLUSH);
            if (FAILED(result)) {
                if (result == DXGI_ERROR_DEVICE_REMOVED || result == DXGI_ERROR_DEVICE_RESET)
                    checkHr(result, "GetData(GPU timing)");
                if (logger_)
                    logger_->warning("GPU timestamp reading disabled: " +
                                     hexCode(static_cast<unsigned long>(result)));
                reset();
                return false;
            }
            return result == S_OK;
        };
        if (!read(slot.disjoint.Get(), &disjoint, sizeof(disjoint)) ||
            !read(slot.first.Get(), &first, sizeof(first)) ||
            !read(slot.last.Get(), &last, sizeof(last)))
            continue;
        slot.pending = false;
        if (!disjoint.Disjoint && disjoint.Frequency && last >= first)
            metrics_.add(static_cast<double>(last - first) * 1000.0 /
                         static_cast<double>(disjoint.Frequency));
    }
}
void GpuTimer::reset() noexcept {
    logger_ = nullptr;
    slots_ = {};
    metrics_.clear();
    active_ = -1;
    enabled_ = false;
}
} // namespace aetherwall
