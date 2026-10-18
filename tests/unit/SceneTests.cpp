#include "scenes/ScenePackage.h"
#include "scenes/SceneParser.h"
#include "system/RenderDemand.h"

#include <archive.h>
#include <archive_entry.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <atomic>
#include <chrono>
#include <fstream>

namespace aetherwall {
namespace {
using Json = nlohmann::json;
const std::filesystem::path bundled(AETHERWALL_BUNDLED_DIR);

Bytes bytesOf(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(stream), {});
}

Json example() {
    std::ifstream stream(bundled / "gradient" / "manifest.json");
    return Json::parse(stream);
}

template <typename Operation> void rejectsWith(Operation operation, const std::string& reason) {
    try {
        operation();
        FAIL() << "Expected rejection: " << reason;
    } catch (const std::exception& error) {
        EXPECT_NE(std::string(error.what()).find(reason), std::string::npos) << error.what();
    }
}

class SceneFiles : public testing::Test {
  protected:
    std::filesystem::path root;
    void SetUp() override {
        static std::atomic_int counter{0};
        root = std::filesystem::temp_directory_path() /
               ("aetherwall-scene-test-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                std::to_string(counter.fetch_add(1)));
        std::filesystem::create_directories(root / "assets");
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }
    void write(const std::filesystem::path& path, const Bytes& bytes) {
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(stream.good());
    }
    void scene(const Json& manifest) {
        const auto text = manifest.dump();
        write(root / "manifest.json", Bytes(text.begin(), text.end()));
        write(root / "assets" / "preview.png",
              bytesOf(bundled / "gradient" / "assets" / "preview.png"));
    }
    std::filesystem::path zip(const std::vector<std::pair<std::string, Bytes>>& files) {
        Bytes encoded(4 * 1024 * 1024);
        std::size_t used = 0;
        std::unique_ptr<archive, decltype(&archive_write_free)> writer(archive_write_new(),
                                                                       &archive_write_free);
        if (!writer || archive_write_set_format_zip(writer.get()) != ARCHIVE_OK ||
            archive_write_open_memory(writer.get(), encoded.data(), encoded.size(), &used) !=
                ARCHIVE_OK)
            throw std::runtime_error("Test ZIP writer could not open memory");
        for (const auto& [name, bytes] : files) {
            std::unique_ptr<archive_entry, decltype(&archive_entry_free)> entry(
                archive_entry_new(), &archive_entry_free);
            archive_entry_set_pathname(entry.get(), name.c_str());
            archive_entry_set_filetype(entry.get(), AE_IFREG);
            archive_entry_set_perm(entry.get(), 0644);
            archive_entry_set_size(entry.get(), static_cast<la_int64_t>(bytes.size()));
            if (archive_write_header(writer.get(), entry.get()) != ARCHIVE_OK ||
                archive_write_data(writer.get(), bytes.data(), bytes.size()) !=
                    static_cast<la_ssize_t>(bytes.size()))
                throw std::runtime_error("Test ZIP writer failed");
        }
        if (archive_write_close(writer.get()) != ARCHIVE_OK)
            throw std::runtime_error("Test ZIP close failed");
        encoded.resize(used);
        const auto path = root / "test.awscene";
        write(path, encoded);
        return path;
    }
    std::vector<std::pair<std::string, Bytes>> zipFiles() {
        const auto manifest = example().dump();
        return {{"manifest.json", Bytes(manifest.begin(), manifest.end())},
                {"assets/preview.png", bytesOf(bundled / "gradient" / "assets" / "preview.png")}};
    }
};

TEST(Scenes, AllBundledManifestsLoadWithCorrectAnimationClassification) {
    for (const auto* name :
         {"stars", "rain", "gradient", "mountains", "minimal_particles", "still"}) {
        SCOPED_TRACE(name);
        const auto package = loadScenePackage(bundled / name);
        EXPECT_EQ(package->scene.animated(), std::string_view(name) != "still");
        EXPECT_FALSE(package->assets.empty());
    }
}

TEST(Scenes, RejectsInvalidMetadataAndVersions) {
    for (const auto* field : {"id", "author", "license", "preview", "canvas", "layers"}) {
        auto json = example();
        json.erase(field);
        EXPECT_ANY_THROW(parseSceneManifest(json.dump())) << field;
    }
    for (const auto& value : {Json(2), Json(1.0), Json("1"), Json(-1)}) {
        auto json = example();
        json["schemaVersion"] = value;
        EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    }
    auto json = example();
    json["id"] = "invalid";
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
}

TEST(Scenes, RejectsUnknownFieldsTypesEffectsAndDuplicateLayerIds) {
    for (const auto* kind : {"video", "html", "native", "shader"}) {
        auto json = example();
        json["layers"][0]["type"] = kind;
        EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    }
    auto json = example();
    json["layers"][0]["effect"] = "downloaded.hlsl";
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json = example();
    json["layers"].push_back(json["layers"][0]);
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json = example();
    json["executable"] = "test.exe";
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json = example();
    json["layers"][0]["source"] = "assets/unused.png";
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
}

TEST(Scenes, RejectsMalformedUtf8DuplicateKeysAndDeepJson) {
    auto invalidUtf8 = example().dump();
    invalidUtf8.insert(invalidUtf8.find("Floating"), 1, '\xff');
    EXPECT_ANY_THROW(parseSceneManifest(invalidUtf8));
    auto duplicate = example().dump();
    duplicate.insert(1, "\"schemaVersion\":1,");
    rejectsWith([&] { parseSceneManifest(duplicate); }, "Duplicate JSON key");
    rejectsWith([] { parseSceneManifest(std::string(40, '[') + "0" + std::string(40, ']')); },
                "nesting");
    EXPECT_ANY_THROW(parseSceneManifest(std::string(SceneLimits::manifestBytes + 1, ' ')));
}

TEST(Scenes, RejectsLayerAndParticleBudgetsAndInvalidTransforms) {
    auto json = example();
    json["layers"][0]["opacity"] = -0.1;
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json = example();
    json["layers"][0]["transform"] = {{"scale", {0, 1}}};
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json = example();
    json["layers"] = Json::array();
    for (int index = 0; index < 65; ++index)
        json["layers"].push_back({{"id", std::to_string(index)}, {"type", "gradient"}});
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
    json["layers"] = Json::array();
    for (int index = 0; index < 3; ++index)
        json["layers"].push_back({{"id", std::to_string(index)},
                                  {"type", "particles"},
                                  {"preset", "stars"},
                                  {"parameters", {{"count", 2048}}}});
    EXPECT_ANY_THROW(parseSceneManifest(json.dump()));
}

TEST(Scenes, LayerOrderIsStableAndStaticClassificationIncludesEffects) {
    auto json = example();
    json["layers"] =
        Json::array({{{"id", "a"}, {"type", "gradient"}, {"speed", 0}, {"zIndex", 4}},
                     {{"id", "b"}, {"type", "gradient"}, {"speed", 0}, {"zIndex", -2}},
                     {{"id", "c"}, {"type", "gradient"}, {"speed", 0}, {"zIndex", 4}}});
    auto scene = parseSceneManifest(json.dump());
    EXPECT_EQ(scene.layers[0].id, "b");
    EXPECT_EQ(scene.layers[1].id, "a");
    EXPECT_EQ(scene.layers[2].id, "c");
    EXPECT_FALSE(scene.animated());
    json["layers"][0]["effect"] = "pulse";
    EXPECT_TRUE(parseSceneManifest(json.dump()).animated());
    json["layers"][0]["opacity"] = 0;
    EXPECT_FALSE(parseSceneManifest(json.dump()).animated());
}

TEST(Scenes, RejectsUnsafePortablePaths) {
    for (const auto* path : {"../escape.png", "assets/../escape.png", "/absolute.png", "C:/x.png",
                             "assets\\x.png", "assets//x.png", "assets/./x.png", "assets/con.png",
                             "assets/a.png:exe", "assets/trailing./x.png", "assets/lpt1.jpg"})
        EXPECT_THROW(normalizedAssetPath(path), std::invalid_argument) << path;
    EXPECT_EQ(normalizedAssetPath("assets/Sub/IMAGE.PNG"), "assets/sub/image.png");
}

TEST_F(SceneFiles, DirectoryAndZipHaveTheSameScene) {
    scene(example());
    const auto directory = loadScenePackage(root);
    const auto packed = loadScenePackage(zip(zipFiles()));
    EXPECT_EQ(directory->scene.id, packed->scene.id);
    EXPECT_EQ(directory->assets, packed->assets);
}

TEST_F(SceneFiles, RejectsZipSlipAndWindowsCaseCollisions) {
    auto files = zipFiles();
    files.push_back({"../escape.png", Bytes{1}});
    EXPECT_ANY_THROW(loadScenePackage(zip(files)));
    files = zipFiles();
    files.push_back({"ASSETS/PREVIEW.PNG", files[1].second});
    EXPECT_ANY_THROW(loadScenePackage(zip(files)));
    EXPECT_FALSE(std::filesystem::exists(root.parent_path() / "escape.png"));
}

TEST_F(SceneFiles, RejectsExecutableAssetsAndCompressionBombs) {
    auto files = zipFiles();
    files.push_back({"assets/payload.dll", Bytes{1}});
    EXPECT_ANY_THROW(loadScenePackage(zip(files)));
    // Test compressed expansion independently of the entropy of bundled artwork.
    files = {{"assets/bomb.png", Bytes(3 * 1024 * 1024, 0)}};
    const auto path = zip(files);
    rejectsWith([&] { loadScenePackage(path); }, "compression ratio");
}

TEST_F(SceneFiles, RejectsMissingAndMalformedImages) {
    auto json = example();
    json["layers"][0] = {{"id", "image"}, {"type", "image"}, {"source", "assets/missing.png"}};
    scene(json);
    EXPECT_ANY_THROW(loadScenePackage(root));
    scene(example());
    write(root / "assets" / "preview.png", Bytes{1, 2, 3});
    EXPECT_ANY_THROW(loadScenePackage(root));
}

TEST_F(SceneFiles, RejectsLinksAndNeverReadsOutsideDirectory) {
    scene(example());
    std::error_code error;
    std::filesystem::create_symlink(root / "manifest.json", root / "assets" / "link.png", error);
    if (error)
        GTEST_SKIP() << "Symlink creation unavailable: " << error.message();
    EXPECT_ANY_THROW(loadScenePackage(root));
}

TEST_F(SceneFiles, RejectsDecodedBudgetBeforePixelAllocation) {
    scene(example());
    auto png = bytesOf(root / "assets" / "preview.png");
    // A declared 8192x8192 RGBA image is within dimension limits but exceeds 128 MiB.
    for (const std::size_t offset : {16u, 20u}) {
        png[offset] = 0;
        png[offset + 1] = 0;
        png[offset + 2] = 0x20;
        png[offset + 3] = 0;
    }
    write(root / "assets" / "preview.png", png);
    rejectsWith([&] { loadScenePackage(root); }, "memory budget");
}

TEST(Images, RejectsHugeDimensionsBeforePixelAllocation) {
    auto png = bytesOf(bundled / "gradient" / "assets" / "preview.png");
    png[16] = 0x7f;
    EXPECT_THROW(inspectImage("assets/large.png", png), std::invalid_argument);
    EXPECT_THROW(inspectImage("assets/wrong.jpg", png), std::invalid_argument);
    EXPECT_THROW(inspectImage("assets/fake.webp", Bytes{1, 2, 3}), std::invalid_argument);
}

TEST(Geometry, CoverContainStretchPreserveExpectedAxes) {
    Canvas canvas;
    Layer layer;
    const ImageInfo size{1920, 1080};
    auto geometry = layerGeometry(canvas, layer, 1000, 1000, 0, size);
    EXPECT_NEAR(geometry.axisX[0], 1920.0f / 1080.0f, 0.0001f);
    EXPECT_NEAR(geometry.axisY[1], -1, 0.0001f);
    canvas.fit = Fit::Contain;
    geometry = layerGeometry(canvas, layer, 1000, 1000, 0, size);
    EXPECT_NEAR(geometry.axisX[0], 1, 0.0001f);
    EXPECT_NEAR(geometry.axisY[1], -0.5625f, 0.0001f);
    canvas.fit = Fit::Stretch;
    geometry = layerGeometry(canvas, layer, 1000, 1000, 0, size);
    EXPECT_FLOAT_EQ(geometry.axisX[0], 1);
    EXPECT_FLOAT_EQ(geometry.axisY[1], -1);
}

TEST(Geometry, CropsWideImageAndAppliesRotationAndMotion) {
    Canvas canvas;
    canvas.width = canvas.height = 100;
    Layer layer;
    layer.type = LayerType::Image;
    layer.position = {50, 50};
    layer.rotation = 90;
    layer.amplitude = {10, 0};
    layer.motionSpeed = 1;
    auto geometry = layerGeometry(canvas, layer, 100, 100, 1.57079632679, {200, 100});
    EXPECT_FLOAT_EQ(geometry.uv[0], 0.25f);
    EXPECT_FLOAT_EQ(geometry.uv[2], 0.5f);
    EXPECT_NEAR(geometry.center[0], 0.2f, 0.0001f);
    EXPECT_NEAR(geometry.axisX[1], -1, 0.0001f);
}

TEST(RenderDemand, StaticSceneStopsUntilInvalidatedAndAnimationKeepsDrawing) {
    RenderDemand demand;
    EXPECT_TRUE(demand.needsFrame(false));
    demand.presented();
    for (int tick = 0; tick < 1000; ++tick)
        EXPECT_FALSE(demand.needsFrame(false));
    EXPECT_TRUE(demand.needsFrame(true));
    demand.invalidate();
    EXPECT_TRUE(demand.needsFrame(false));
}

TEST(RenderDemand, OccludedOrFailedPresentRetainsPendingRedraw) {
    RenderDemand demand;
    EXPECT_TRUE(demand.needsFrame(false));
    EXPECT_TRUE(demand.needsFrame(false));
    demand.presented();
    EXPECT_FALSE(demand.needsFrame(false));
}
} // namespace
} // namespace aetherwall
