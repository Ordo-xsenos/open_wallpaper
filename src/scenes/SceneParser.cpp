#include "scenes/SceneParser.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

namespace aetherwall {
namespace {
using Json = nlohmann::json;

void fields(const Json& object, std::initializer_list<std::string_view> allowed) {
    if (!object.is_object()) {
        throw std::invalid_argument("Expected a JSON object");
    }
    for (const auto& item : object.items()) {
        if (std::find(allowed.begin(), allowed.end(), item.key()) == allowed.end()) {
            throw std::invalid_argument("Unknown field: " + item.key());
        }
    }
}

std::string text(const Json& value, std::size_t maximum = 128) {
    if (!value.is_string()) {
        throw std::invalid_argument("Expected a string");
    }
    auto result = value.get<std::string>();
    if (result.empty() || result.size() > maximum ||
        std::any_of(result.begin(), result.end(), [](unsigned char c) { return c < 32; })) {
        throw std::invalid_argument("Invalid string length/control character");
    }
    return result;
}

float number(const Json& value, float minimum, float maximum, bool integer = false) {
    if (!value.is_number() || (integer && !value.is_number_integer())) {
        throw std::invalid_argument("Expected a bounded number");
    }
    const double result = value.get<double>();
    if (!std::isfinite(result) || result < minimum || result > maximum) {
        throw std::invalid_argument("Numeric value out of range");
    }
    return static_cast<float>(result);
}

template <std::size_t Size>
std::array<float, Size> vector(const Json& value, float minimum, float maximum) {
    if (!value.is_array() || value.size() != Size) {
        throw std::invalid_argument("Vector has incorrect dimensions");
    }
    std::array<float, Size> result{};
    for (std::size_t index = 0; index < Size; ++index) {
        result[index] = number(value[index], minimum, maximum);
    }
    return result;
}

bool uuid(std::string_view value) {
    if (value.size() != 36) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        const char c = value[index];
        if (index == 8 || index == 13 || index == 18 || index == 23) {
            if (c != '-')
                return false;
        } else if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return false;
        }
    }
    return true;
}
} // namespace

std::string normalizedAssetPath(std::string_view path) {
    if (path.empty() || path.size() > 200 || path.front() == '/' || path.back() == '/') {
        throw std::invalid_argument("Invalid package path");
    }
    std::string result(path);
    for (auto& c : result) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.' || c == '/')) {
            throw std::invalid_argument("Package paths must use portable ASCII filenames");
        }
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    }
    std::size_t start = 0;
    while (start < result.size()) {
        const auto end = result.find('/', start);
        const auto part = result.substr(start, end == std::string::npos ? end : end - start);
        const auto stem = part.substr(0, part.find('.'));
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || stem == "con" ||
            stem == "nul" || stem == "prn" || stem == "aux" ||
            (stem.size() == 4 && (stem.starts_with("com") || stem.starts_with("lpt")) &&
             stem[3] >= '0' && stem[3] <= '9')) {
            throw std::invalid_argument("Unsafe package path component");
        }
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
    return result;
}

SceneDefinition parseSceneManifest(std::string_view manifest) {
    if (manifest.empty() || manifest.size() > SceneLimits::manifestBytes) {
        throw std::invalid_argument("Manifest exceeds size limit or is empty");
    }
    std::vector<std::set<std::string>> keys;
    const auto callback = [&keys](int depth, Json::parse_event_t event, Json& value) {
        if (depth > 16)
            throw std::invalid_argument("JSON nesting exceeds limit");
        if (event == Json::parse_event_t::object_start)
            keys.emplace_back();
        if (event == Json::parse_event_t::key &&
            !keys.back().insert(value.get<std::string>()).second) {
            throw std::invalid_argument("Duplicate JSON key");
        }
        if (event == Json::parse_event_t::object_end)
            keys.pop_back();
        return true;
    };
    const auto root = Json::parse(manifest, callback);
    fields(root, {"schemaVersion", "id", "title", "author", "license", "tags", "preview", "canvas",
                  "fpsHint", "layers"});
    if (number(root.at("schemaVersion"), 1, 1, true) != 1) {
        throw std::invalid_argument("Unsupported scene schema version");
    }
    SceneDefinition scene;
    scene.id = text(root.at("id"), 36);
    if (!uuid(scene.id))
        throw std::invalid_argument("Invalid scene UUID");
    scene.title = text(root.at("title"));
    scene.author = text(root.at("author"));
    scene.license = text(root.at("license"), 64);
    scene.preview = normalizedAssetPath(text(root.at("preview"), 200));
    if (root.contains("tags")) {
        const auto& tags = root.at("tags");
        if (!tags.is_array() || tags.size() > 32)
            throw std::invalid_argument("Invalid tags");
        for (const auto& tag : tags)
            scene.tags.push_back(text(tag, 32));
    }
    if (root.contains("fpsHint")) {
        scene.fpsHint = static_cast<int>(number(root.at("fpsHint"), 15, 60, true));
        if (scene.fpsHint != 15 && scene.fpsHint != 30 && scene.fpsHint != 60)
            throw std::invalid_argument("fpsHint must be 15, 30 or 60");
    }
    const auto& canvas = root.at("canvas");
    fields(canvas, {"width", "height", "fit", "background"});
    scene.canvas.width = static_cast<int>(number(canvas.at("width"), 1, 8192, true));
    scene.canvas.height = static_cast<int>(number(canvas.at("height"), 1, 8192, true));
    const auto fit = text(canvas.at("fit"));
    if (fit == "cover")
        scene.canvas.fit = Fit::Cover;
    else if (fit == "contain")
        scene.canvas.fit = Fit::Contain;
    else if (fit == "stretch")
        scene.canvas.fit = Fit::Stretch;
    else
        throw std::invalid_argument("Unknown canvas fit");
    if (canvas.contains("background"))
        scene.canvas.background = vector<4>(canvas.at("background"), 0, 1);
    const auto& layers = root.at("layers");
    if (!layers.is_array() || layers.empty() || layers.size() > SceneLimits::layers)
        throw std::invalid_argument("Invalid layer count");
    std::set<std::string> ids;
    int particles = 0;
    for (const auto& entry : layers) {
        fields(entry, {"id", "type", "source", "zIndex", "opacity", "tint", "transform", "motion",
                       "speed", "colorA", "colorB", "preset", "parameters", "effect"});
        Layer layer;
        layer.id = text(entry.at("id"), 64);
        if (!ids.insert(layer.id).second)
            throw std::invalid_argument("Duplicate layer id");
        layer.position = {static_cast<float>(scene.canvas.width) / 2,
                          static_cast<float>(scene.canvas.height) / 2};
        const auto type = text(entry.at("type"));
        if (type == "image") {
            layer.type = LayerType::Image;
            layer.source = normalizedAssetPath(text(entry.at("source"), 200));
            layer.speed = 0;
        } else if (type == "gradient")
            layer.type = LayerType::Gradient;
        else if (type == "particles") {
            layer.type = LayerType::Particles;
            const auto preset = text(entry.at("preset"));
            if (preset == "stars")
                layer.preset = ParticlePreset::Stars;
            else if (preset == "rain")
                layer.preset = ParticlePreset::Rain;
            else if (preset == "snow")
                layer.preset = ParticlePreset::Snow;
            else
                throw std::invalid_argument("Unknown particle preset");
            if (entry.contains("parameters")) {
                const auto& params = entry.at("parameters");
                fields(params, {"count", "speed", "size"});
                if (params.contains("count"))
                    layer.count = static_cast<int>(number(params.at("count"), 1, 2048, true));
                if (params.contains("speed"))
                    layer.speed = number(params.at("speed"), 0, 10);
                if (params.contains("size"))
                    layer.size = number(params.at("size"), 1, 64);
            }
            particles += layer.count;
            if (particles > SceneLimits::particles)
                throw std::invalid_argument("Particle budget exceeded");
        } else
            throw std::invalid_argument("Unsupported layer type");
        if ((entry.contains("source") && layer.type != LayerType::Image) ||
            ((entry.contains("preset") || entry.contains("parameters")) &&
             layer.type != LayerType::Particles) ||
            ((entry.contains("colorA") || entry.contains("colorB")) &&
             layer.type != LayerType::Gradient))
            throw std::invalid_argument("Field does not apply to this layer type");
        if (entry.contains("zIndex"))
            layer.zIndex = static_cast<int>(number(entry.at("zIndex"), -10000, 10000, true));
        if (entry.contains("opacity"))
            layer.opacity = number(entry.at("opacity"), 0, 1);
        if (entry.contains("tint"))
            layer.tint = vector<4>(entry.at("tint"), 0, 1);
        if (entry.contains("speed")) {
            if (layer.type != LayerType::Gradient)
                throw std::invalid_argument("speed is for gradient layers");
            layer.speed = number(entry.at("speed"), 0, 10);
        }
        if (entry.contains("colorA"))
            layer.colorA = vector<4>(entry.at("colorA"), 0, 1);
        if (entry.contains("colorB"))
            layer.colorB = vector<4>(entry.at("colorB"), 0, 1);
        if (entry.contains("effect")) {
            const auto effect = text(entry.at("effect"));
            if (effect == "pulse")
                layer.pulse = true;
            else if (effect != "none")
                throw std::invalid_argument("Unknown trusted effect");
        }
        if (entry.contains("transform")) {
            const auto& transform = entry.at("transform");
            fields(transform, {"position", "scale", "rotation"});
            if (transform.contains("position"))
                layer.position = vector<2>(transform.at("position"), -32768, 32768);
            if (transform.contains("scale"))
                layer.scale = vector<2>(transform.at("scale"), 0.01f, 16);
            if (transform.contains("rotation"))
                layer.rotation = number(transform.at("rotation"), -3600, 3600);
        }
        if (entry.contains("motion")) {
            const auto& motion = entry.at("motion");
            fields(motion, {"amplitude", "speed"});
            layer.amplitude = vector<2>(motion.at("amplitude"), -8192, 8192);
            layer.motionSpeed = number(motion.at("speed"), 0, 10);
        }
        scene.layers.push_back(std::move(layer));
    }
    std::stable_sort(
        scene.layers.begin(), scene.layers.end(),
        [](const Layer& left, const Layer& right) { return left.zIndex < right.zIndex; });
    return scene;
}
} // namespace aetherwall
