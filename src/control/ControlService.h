#pragma once
#include "storage/LibraryStore.h"
#include "storage/SettingsStore.h"
#include <functional>

namespace aetherwall {
// Runtime callbacks execute only on Core's message-loop thread.
struct RuntimeControl {
    std::function<Json()> status, monitors, metrics;
    std::function<void(const Json&)> updateSettings;
    std::function<std::function<void()>(const std::vector<std::string>&,
                                        const std::filesystem::path&)>
        prepareApply;
    std::function<void(bool, const std::string&)> pause;
    std::function<void()> stop, reload;
    std::function<bool(const std::filesystem::path&)> active;
};
class ControlService {
  public:
    ControlService(const std::filesystem::path& directory, RuntimeControl runtime);
    std::string handle(std::string_view bytes) noexcept;
    void seed(const std::filesystem::path& bundled);
    const Json& settings() const { return settings_; }
    LibraryStore& library() { return library_; }
    const std::string& warning() const { return store_.warning(); }

  private:
    Json dispatch(const Request& request);
    RuntimeControl runtime_;
    SettingsStore store_;
    Json settings_;
    LibraryStore library_;
};
} // namespace aetherwall
