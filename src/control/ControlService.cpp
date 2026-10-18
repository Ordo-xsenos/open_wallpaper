#include "control/ControlService.h"
#include <set>

namespace aetherwall {
ControlService::ControlService(const std::filesystem::path& directory, RuntimeControl runtime)
    : runtime_(std::move(runtime)), store_(directory), settings_(store_.load()),
      library_(directory) {}
void ControlService::seed(const std::filesystem::path& bundled) {
    if (!std::filesystem::is_directory(bundled))
        return;
    // First launch only: deleting an example must not reinstall it on every restart.
    const auto marker = library_.directory() / "bundled-v1.seeded";
    if (std::filesystem::exists(marker))
        return;
    for (const auto& entry : std::filesystem::directory_iterator(bundled)) {
        if (!entry.is_directory())
            continue;
        try {
            library_.importScene(entry.path());
        } catch (const std::invalid_argument& error) {
            if (std::string(error.what()) != "This scene ID is already installed")
                throw;
        }
    }
    atomicWrite(marker, "seeded\n");
}
std::string ControlService::handle(std::string_view bytes) noexcept {
    Request request;
    try {
        request = parseRequest(bytes);
    } catch (const std::exception& error) {
        return failure("", "invalid_request", error.what());
    }
    try {
        return success(request.id, dispatch(request));
    } catch (const std::invalid_argument& error) {
        return failure(request.id, "invalid_request", error.what());
    } catch (const Json::exception& error) {
        return failure(request.id, "invalid_request", error.what());
    } catch (const std::exception& error) {
        return failure(request.id, "operation_failed", error.what());
    }
}

Json ControlService::dispatch(const Request& request) {
    const auto& p = request.params;
    const auto& command = request.command;
    if (command == "ping") {
        onlyFields(p, {});
        return {{"name", "AetherWall.Core"}, {"protocolVersion", 1}};
    }
    if (command == "getSettings") {
        onlyFields(p, {});
        return settings_;
    }
    if (command == "getStatus") {
        onlyFields(p, {});
        auto result = runtime_.status();
        result["settingsWarning"] = warning();
        return result;
    }
    if (command == "getMonitors") {
        onlyFields(p, {});
        return {{"monitors", runtime_.monitors()}, {"assignments", library_.assignments()}};
    }
    if (command == "getMetrics") {
        onlyFields(p, {});
        return runtime_.metrics();
    }
    if (command == "listInstalledScenes") {
        onlyFields(p, {});
        return library_.list();
    }
    if (command == "updateSettings") {
        onlyFields(p, {"settings"});
        const auto next = validateSettings(p.at("settings"));
        // Persist first; runtime validates settings again and rolls registry changes back on
        // failure.
        store_.save(next);
        try {
            runtime_.updateSettings(next);
        } catch (...) {
            store_.save(settings_);
            throw;
        }
        settings_ = next;
        return settings_;
    }
    if (command == "importScene") {
        onlyFields(p, {"path"});
        return {{"sceneId", library_.importScene(textPath(boundedText(p.at("path"), 4096)))}};
    }
    if (command == "getDownloadHistory") {
        onlyFields(p, {});
        return library_.downloadHistory();
    }
    if (command == "importDownloadedScene") {
        onlyFields(p, {"path", "sceneId", "sha256", "source", "size"});
        if (!p.at("size").is_number_unsigned() && !p.at("size").is_number_integer())
            throw std::invalid_argument("Invalid download size");
        const auto size = p.at("size").get<std::int64_t>();
        if (size <= 0 || size > static_cast<std::int64_t>(SceneLimits::packageBytes))
            throw std::invalid_argument("Invalid download size");
        DownloadReceipt receipt{boundedText(p.at("sceneId"), 36), boundedText(p.at("sha256"), 64),
                                boundedText(p.at("source"), 2048),
                                static_cast<std::uintmax_t>(size)};
        if (receipt.sha256.size() != 64 ||
            receipt.sha256.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::invalid_argument("Invalid SHA-256");
        return {
            {"sceneId", library_.importScene(textPath(boundedText(p.at("path"), 4096)), &receipt)}};
    }
    if (command == "setFavorite") {
        onlyFields(p, {"sceneId", "favorite"});
        if (!p.at("favorite").is_boolean())
            throw std::invalid_argument("favorite must be boolean");
        library_.favorite(boundedText(p.at("sceneId"), 64), p.at("favorite").get<bool>());
        return Json::object();
    }
    if (command == "deleteScene") {
        onlyFields(p, {"sceneId"});
        const auto id = boundedText(p.at("sceneId"), 64);
        if (runtime_.active(library_.scenePath(id)))
            throw std::invalid_argument("Scene is active; apply another scene first");
        library_.remove(id);
        return Json::object();
    }
    if (command == "applyScene") {
        onlyFields(p, {"monitorId", "sceneId"});
        const auto selector = boundedText(p.at("monitorId"), 512),
                   id = boundedText(p.at("sceneId"), 64);
        std::vector<std::string> ids;
        for (const auto& monitor : runtime_.monitors()) {
            const auto key = monitor.at("id").get<std::string>();
            if (selector == "*" || key == selector)
                ids.push_back(key);
        }
        if (ids.empty())
            throw std::invalid_argument("Monitor is disconnected or unknown");
        // GPU staging callback is transactional across all selected outputs.
        auto commit = runtime_.prepareApply(ids, library_.scenePath(id));
        library_.assign(ids, id);
        commit();
        return {{"monitorIds", ids}, {"sceneId", id}};
    }
    if (command == "pause" || command == "resume") {
        onlyFields(p, {"monitorId"});
        runtime_.pause(command == "pause", boundedText(p.at("monitorId"), 512));
        return Json::object();
    }
    if (command == "stop") {
        onlyFields(p, {});
        runtime_.stop();
        return Json::object();
    }
    if (command == "reloadDisplayTopology") {
        onlyFields(p, {});
        runtime_.reload();
        return Json::object();
    }
    throw std::invalid_argument("Unknown command");
}
} // namespace aetherwall
