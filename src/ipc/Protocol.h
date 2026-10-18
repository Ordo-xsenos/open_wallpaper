#pragma once
#include <cstdint>
#include <nlohmann/json.hpp>
#include <span>

namespace aetherwall {
using Json = nlohmann::json;
constexpr std::size_t ipcMessageLimit = 256 * 1024;
struct Request {
    std::string id, command;
    Json params;
};
Json parseJson(std::string_view bytes, std::size_t maximum = ipcMessageLimit);
Request parseRequest(std::string_view bytes);
std::string success(const std::string& id, Json result);
std::string failure(const std::string& id, const std::string& code, const std::string& message);
std::vector<std::uint8_t> frameMessage(std::string_view payload);
std::uint32_t frameLength(std::span<const std::uint8_t> header);
void onlyFields(const Json& object, std::initializer_list<std::string_view> fields);
std::string boundedText(const Json& value, std::size_t maximum = 512, bool allowEmpty = false);
} // namespace aetherwall
