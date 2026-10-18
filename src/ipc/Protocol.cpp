#include "ipc/Protocol.h"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace aetherwall {
void onlyFields(const Json& object, std::initializer_list<std::string_view> fields) {
    if (!object.is_object())
        throw std::invalid_argument("Expected a JSON object");
    for (const auto& entry : object.items())
        if (std::find(fields.begin(), fields.end(), entry.key()) == fields.end())
            throw std::invalid_argument("Unknown field: " + entry.key());
}
std::string boundedText(const Json& value, std::size_t maximum, bool allowEmpty) {
    if (!value.is_string())
        throw std::invalid_argument("Expected text");
    auto text = value.get<std::string>();
    if ((!allowEmpty && text.empty()) || text.size() > maximum ||
        std::any_of(text.begin(), text.end(), [](unsigned char c) { return c < 32; }))
        throw std::invalid_argument("Invalid text length or control characters");
    return text;
}
Json parseJson(std::string_view bytes, std::size_t maximum) {
    if (bytes.empty() || bytes.size() > maximum)
        throw std::invalid_argument("Message size limit exceeded");
    std::vector<std::set<std::string>> keys;
    return Json::parse(bytes, [&](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 24)
            throw std::invalid_argument("JSON nesting limit exceeded");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(value.get<std::string>()).second)
            throw std::invalid_argument("Duplicate JSON key");
        if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    });
}
Request parseRequest(std::string_view bytes) {
    const auto value = parseJson(bytes);
    onlyFields(value, {"version", "requestId", "command", "params"});
    if (!value.at("version").is_number_integer() || value.at("version") != 1)
        throw std::invalid_argument("Unsupported protocol version");
    Request result{boundedText(value.at("requestId"), 64), boundedText(value.at("command"), 64),
                   value.at("params")};
    if (!result.params.is_object())
        throw std::invalid_argument("params must be an object");
    return result;
}
std::string success(const std::string& id, Json result) {
    auto bytes =
        Json{{"version", 1}, {"requestId", id}, {"ok", true}, {"result", std::move(result)}}.dump();
    if (bytes.size() > ipcMessageLimit)
        throw std::runtime_error("Response size limit exceeded");
    return bytes;
}
std::string failure(const std::string& id, const std::string& code, const std::string& message) {
    return Json{{"version", 1},
                {"requestId", id},
                {"ok", false},
                {"error", {{"code", code}, {"message", message.substr(0, 1024)}}}}
        .dump(-1, ' ', false, Json::error_handler_t::replace);
}
std::uint32_t frameLength(std::span<const std::uint8_t> header) {
    if (header.size() != 4)
        throw std::invalid_argument("Expected four-byte frame header");
    const std::uint32_t length = header[0] | (static_cast<std::uint32_t>(header[1]) << 8) |
                                 (static_cast<std::uint32_t>(header[2]) << 16) |
                                 (static_cast<std::uint32_t>(header[3]) << 24);
    if (!length || length > ipcMessageLimit)
        throw std::invalid_argument("Invalid frame length");
    return length;
}
std::vector<std::uint8_t> frameMessage(std::string_view payload) {
    if (payload.empty() || payload.size() > ipcMessageLimit)
        throw std::invalid_argument("Invalid frame length");
    const auto size = static_cast<std::uint32_t>(payload.size());
    std::vector<std::uint8_t> result{
        static_cast<std::uint8_t>(size), static_cast<std::uint8_t>(size >> 8),
        static_cast<std::uint8_t>(size >> 16), static_cast<std::uint8_t>(size >> 24)};
    result.insert(result.end(), payload.begin(), payload.end());
    return result;
}
} // namespace aetherwall
