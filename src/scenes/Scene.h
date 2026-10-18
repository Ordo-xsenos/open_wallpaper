#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace aetherwall {

using Color = std::array<float, 4>;
using Vec2 = std::array<float, 2>;
using Bytes = std::vector<std::uint8_t>;
enum class Fit { Cover, Contain, Stretch };
enum class LayerType { Gradient, Image, Particles };
enum class ParticlePreset { Stars, Rain, Snow };

struct SceneLimits {
    static constexpr std::size_t packageBytes = 32 * 1024 * 1024;
    static constexpr std::size_t expandedBytes = 64 * 1024 * 1024;
    static constexpr std::size_t manifestBytes = 256 * 1024;
    static constexpr std::size_t decodedBytes = 128 * 1024 * 1024;
    static constexpr int imageDimension = 8192;
    static constexpr int layers = 64;
    static constexpr int particles = 4096;
    static constexpr int entries = 256;
};

struct Canvas {
    int width = 1920;
    int height = 1080;
    Fit fit = Fit::Cover;
    Color background{0.02f, 0.03f, 0.06f, 1.0f};
};

struct Layer {
    std::string id;
    LayerType type = LayerType::Gradient;
    std::string source;
    int zIndex = 0;
    float opacity = 1.0f;
    Color tint{1, 1, 1, 1};
    Color colorA{0.025f, 0.05f, 0.16f, 1};
    Color colorB{0.08f, 0.40f, 0.56f, 1};
    Vec2 position{960, 540};
    Vec2 scale{1, 1};
    Vec2 amplitude{0, 0};
    float rotation = 0;
    float motionSpeed = 0;
    float speed = 0.65f;
    float size = 3;
    int count = 120;
    ParticlePreset preset = ParticlePreset::Stars;
    bool pulse = false;
    bool animated() const;
};

struct SceneDefinition {
    std::string id;
    std::string title;
    std::string author;
    std::string license;
    std::string preview;
    std::vector<std::string> tags;
    int fpsHint = 30;
    Canvas canvas;
    std::vector<Layer> layers;
    bool animated() const;
};

struct ImageInfo {
    int width;
    int height;
};

struct ScenePackage {
    std::string manifest; // Original validated bytes for safe managed-library snapshots.
    SceneDefinition scene;
    std::map<std::string, Bytes> assets;
    std::map<std::string, ImageInfo> images;
};

class IScene {
  public:
    virtual ~IScene() = default;
    virtual const ScenePackage& package() const = 0;
    virtual bool animated() const = 0;
};

class SceneRuntime final : public IScene {
  public:
    explicit SceneRuntime(std::shared_ptr<const ScenePackage> package);
    const ScenePackage& package() const override { return *package_; }
    bool animated() const override { return package_->scene.animated(); }
    std::shared_ptr<const ScenePackage> source() const { return package_; }

  private:
    std::shared_ptr<const ScenePackage> package_;
};

struct LayerGeometry {
    Vec2 center;
    Vec2 axisX;
    Vec2 axisY;
    std::array<float, 4> uv{0, 0, 1, 1};
};

LayerGeometry layerGeometry(const Canvas& canvas, const Layer& layer, int viewportWidth,
                            int viewportHeight, double seconds, ImageInfo image);
std::shared_ptr<const ScenePackage> defaultScene();

} // namespace aetherwall
