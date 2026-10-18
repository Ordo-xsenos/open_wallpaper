#include "scenes/Scene.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>

namespace aetherwall {

bool Layer::animated() const {
    return (motionSpeed != 0 && (amplitude[0] != 0 || amplitude[1] != 0)) ||
           (type != LayerType::Image && speed != 0) || pulse;
}

bool SceneDefinition::animated() const {
    return std::any_of(layers.begin(), layers.end(),
                       [](const Layer& layer) { return layer.opacity > 0 && layer.animated(); });
}

SceneRuntime::SceneRuntime(std::shared_ptr<const ScenePackage> package)
    : package_(std::move(package)) {
    if (!package_) {
        throw std::invalid_argument("Scene package is null");
    }
}

LayerGeometry layerGeometry(const Canvas& canvas, const Layer& layer, int viewportWidth,
                            int viewportHeight, double seconds, ImageInfo image) {
    if (viewportWidth <= 0 || viewportHeight <= 0 || canvas.width <= 0 || canvas.height <= 0 ||
        image.width <= 0 || image.height <= 0 || !std::isfinite(seconds)) {
        throw std::invalid_argument("Invalid scene geometry dimensions/time");
    }
    const float cw = static_cast<float>(canvas.width);
    const float ch = static_cast<float>(canvas.height);
    const float vw = static_cast<float>(viewportWidth);
    const float vh = static_cast<float>(viewportHeight);
    float sx = vw / cw;
    float sy = vh / ch;
    if (canvas.fit != Fit::Stretch) {
        sx = sy = canvas.fit == Fit::Cover ? std::max(sx, sy) : std::min(sx, sy);
    }
    float halfWidth = cw * 0.5f;
    float halfHeight = ch * 0.5f;
    LayerGeometry result{};
    if (layer.type == LayerType::Image) {
        const float sourceAspect =
            static_cast<float>(image.width) / static_cast<float>(image.height);
        const float canvasAspect = cw / ch;
        if (canvas.fit == Fit::Cover) {
            if (sourceAspect > canvasAspect) {
                result.uv[2] = canvasAspect / sourceAspect;
                result.uv[0] = (1.0f - result.uv[2]) * 0.5f;
            } else {
                result.uv[3] = sourceAspect / canvasAspect;
                result.uv[1] = (1.0f - result.uv[3]) * 0.5f;
            }
        } else if (canvas.fit == Fit::Contain) {
            if (sourceAspect > canvasAspect) {
                halfHeight = halfWidth / sourceAspect;
            } else {
                halfWidth = halfHeight * sourceAspect;
            }
        }
    }
    const float radians = layer.rotation * std::numbers::pi_v<float> / 180.0f;
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    const float motion = static_cast<float>(std::sin(seconds * layer.motionSpeed));
    result.center = {(layer.position[0] + layer.amplitude[0] * motion - cw * 0.5f) * sx * 2 / vw,
                     -(layer.position[1] + layer.amplitude[1] * motion - ch * 0.5f) * sy * 2 / vh};
    result.axisX = {halfWidth * layer.scale[0] * cosine * sx * 2 / vw,
                    -halfWidth * layer.scale[0] * sine * sy * 2 / vh};
    result.axisY = {-halfHeight * layer.scale[1] * sine * sx * 2 / vw,
                    -halfHeight * layer.scale[1] * cosine * sy * 2 / vh};
    return result;
}

std::shared_ptr<const ScenePackage> defaultScene() {
    auto package = std::make_shared<ScenePackage>();
    package->scene.id = "00000000-0000-4000-8000-000000000001";
    package->scene.title = "Diagnostic gradient";
    package->scene.author = "AetherWall contributors";
    package->scene.license = "MIT";
    Layer layer;
    layer.id = "gradient";
    package->scene.layers.push_back(layer);
    return package;
}

} // namespace aetherwall
