#include "control/ControlService.h"
#include "scenes/ScenePackage.h"
#include "storage/PackageHash.h"
#include "system/Options.h"
#include <fstream>
#include <gtest/gtest.h>
#include <random>
#include <set>
#include <sqlite3.h>

namespace aetherwall {
namespace {
class ControlTest : public testing::Test {
  protected:
    std::filesystem::path directory =
        std::filesystem::temp_directory_path() /
        ("aetherwall-control-" + std::to_string(std::random_device{}()));
    bool committed = false, failPrepare = false, failSettings = false, stopped = false,
         paused = false;
    RuntimeControl runtime() {
        RuntimeControl r;
        r.status = [] { return Json{{"running", true}}; };
        r.monitors = [] {
            return Json::array({{{"id", "test-monitor-1"}}, {{"id", "test-monitor-2"}}});
        };
        r.metrics = [] { return Json::array(); };
        r.updateSettings = [this](const Json&) {
            if (failSettings)
                throw std::runtime_error("Runtime rejected settings");
        };
        r.prepareApply = [this](const auto&, const auto&) -> std::function<void()> {
            if (failPrepare)
                throw std::runtime_error("GPU staging rejected");
            return [this] { committed = true; };
        };
        r.pause = [this](bool value, const auto&) { paused = value; };
        r.stop = [this] { stopped = true; };
        r.reload = [] {};
        r.active = [](const auto&) { return false; };
        return r;
    }
    Json call(ControlService& service, const std::string& command, Json params = Json::object()) {
        return parseJson(service.handle(Json{
            {"version", 1},
            {"requestId", "test"},
            {"command", command},
            {"params", std::move(params)}}.dump()));
    }
    void TearDown() override {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }
};
TEST(Protocol, RejectsDuplicateFieldsInvalidVersionDeepJsonAndOversizedFrames) {
    EXPECT_THROW(
        parseRequest(R"({"version":1,"version":1,"requestId":"a","command":"ping","params":{}})"),
        std::invalid_argument);
    EXPECT_THROW(parseRequest(R"({"version":2,"requestId":"a","command":"ping","params":{}})"),
                 std::invalid_argument);
    EXPECT_THROW(
        parseRequest(R"({"version":1,"requestId":"a","command":"ping","params":[],"extra":1})"),
        std::invalid_argument);
    EXPECT_THROW(parseJson(std::string(30, '[') + "0" + std::string(30, ']')),
                 std::invalid_argument);
    EXPECT_THROW(frameMessage(std::string(ipcMessageLimit + 1, 'a')), std::invalid_argument);
    EXPECT_THROW(frameLength(std::vector<std::uint8_t>{0, 0, 0, 0}), std::invalid_argument);
    EXPECT_THROW(frameLength(std::vector<std::uint8_t>{255, 255, 255, 255}), std::invalid_argument);
}
TEST(Protocol, UsesLittleEndianLengthAndCorrelatesResponses) {
    const auto frame = frameMessage(std::string(257, 'x'));
    EXPECT_EQ(frame[0], 1);
    EXPECT_EQ(frame[1], 1);
    EXPECT_EQ(frameLength(std::span(frame).first(4)), 257u);
    const auto response = parseJson(success("request-42", {{"running", true}}));
    EXPECT_EQ(response.at("requestId"), "request-42");
    EXPECT_EQ(response.at("version"), 1);
    EXPECT_EQ(response.at("ok"), true);
}
TEST_F(ControlTest, ImportsAnIndependentSnapshotAndPersistsFavorites) {
    std::string id;
    {
        LibraryStore library(directory);
        id = library.importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "stars");
        EXPECT_TRUE(std::filesystem::is_regular_file(library.scenePath(id) / "manifest.json"));
        EXPECT_THROW(library.importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "stars"),
                     std::invalid_argument);
        library.favorite(id, true);
        EXPECT_EQ(library.list().at(0).at("favorite"), true);
    }
    {
        LibraryStore library(directory);
        EXPECT_EQ(library.list().size(), 1u);
        EXPECT_EQ(library.list().at(0).at("id"), id);
        EXPECT_TRUE(library.list().at(0).at("favorite").get<bool>());
    }
    EXPECT_TRUE(std::filesystem::is_regular_file(std::filesystem::path(AETHERWALL_BUNDLED_DIR) /
                                                 "stars/manifest.json"));
}
TEST_F(ControlTest, RejectsUnknownIdsAndCannotDeleteAssignedOrExternalFiles) {
    LibraryStore library(directory);
    const auto id = library.importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "rain");
    EXPECT_THROW(library.scenePath("../../outside"), std::invalid_argument);
    EXPECT_THROW(library.scenePath("' OR 1=1 --"), std::invalid_argument);
    library.assign({"first", "second"}, id);
    EXPECT_THROW(library.remove(id), std::invalid_argument);
    EXPECT_EQ(library.assignments().at("second"), id);
}
TEST_F(ControlTest, DeletionRemovesOnlyTheManagedCopy) {
    LibraryStore library(directory);
    const auto source = std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "still";
    const auto id = library.importScene(source);
    const auto path = library.scenePath(id);
    library.remove(id);
    EXPECT_TRUE(library.list().empty());
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_TRUE(std::filesystem::exists(source / "manifest.json"));
}
TEST_F(ControlTest, FailedStagingDoesNotChangeAssignmentsOrCommit) {
    ControlService service(directory, runtime());
    const auto id =
        service.library().importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "stars");
    failPrepare = true;
    auto response = call(service, "applyScene", {{"sceneId", id}, {"monitorId", "*"}});
    EXPECT_FALSE(response.at("ok").get<bool>());
    EXPECT_FALSE(committed);
    EXPECT_TRUE(service.library().assignments().empty());
    failPrepare = false;
    response = call(service, "applyScene", {{"sceneId", id}, {"monitorId", "*"}});
    EXPECT_TRUE(response.at("ok").get<bool>());
    EXPECT_TRUE(committed);
    EXPECT_EQ(service.library().assignments().size(), 2u);
    EXPECT_FALSE(call(service, "applyScene", {{"sceneId", id}, {"monitorId", "missing"}})
                     .at("ok")
                     .get<bool>());
}
TEST_F(ControlTest, SeedsOnceWithoutResurrectingDeletedExamples) {
    ControlService service(directory, runtime());
    service.seed(AETHERWALL_BUNDLED_DIR);
    EXPECT_EQ(service.library().list().size(), 6u);
    const auto id = service.library().list().at(0).at("id").get<std::string>();
    service.library().remove(id);
    service.seed(AETHERWALL_BUNDLED_DIR);
    EXPECT_EQ(service.library().list().size(), 5u);
}
TEST_F(ControlTest, BundledScenesHaveDistinctStaticPreviewArtwork) {
    std::set<Bytes> previews;
    for (const auto& entry : std::filesystem::directory_iterator(AETHERWALL_BUNDLED_DIR)) {
        if (!entry.is_directory())
            continue;
        const auto package = loadScenePackage(entry.path());
        EXPECT_TRUE(previews.insert(package->assets.at(package->scene.preview)).second);
    }
    EXPECT_EQ(previews.size(), 6u);
}
TEST_F(ControlTest, InvalidCommandsHaveExplicitErrorsAndNeverStopCore) {
    ControlService service(directory, runtime());
    EXPECT_FALSE(call(service, "unknown").at("ok").get<bool>());
    EXPECT_FALSE(call(service, "stop", {{"extra", true}}).at("ok").get<bool>());
    EXPECT_FALSE(stopped);
    EXPECT_TRUE(call(service, "pause", {{"monitorId", "*"}}).at("ok").get<bool>());
    EXPECT_TRUE(paused);
    EXPECT_TRUE(call(service, "resume", {{"monitorId", "*"}}).at("ok").get<bool>());
    EXPECT_FALSE(paused);
    EXPECT_TRUE(call(service, "stop").at("ok").get<bool>());
    EXPECT_TRUE(stopped);
}
TEST_F(ControlTest, SettingsPersistAndRejectedRuntimeUpdateRollsBack) {
    ControlService service(directory, runtime());
    auto settings = defaultSettings();
    settings["fps"] = 15;
    EXPECT_TRUE(call(service, "updateSettings", {{"settings", settings}}).at("ok").get<bool>());
    EXPECT_EQ(SettingsStore(directory).load().at("fps"), 15);
    settings["fps"] = 60;
    failSettings = true;
    EXPECT_FALSE(call(service, "updateSettings", {{"settings", settings}}).at("ok").get<bool>());
    EXPECT_EQ(service.settings().at("fps"), 15);
    EXPECT_EQ(SettingsStore(directory).load().at("fps"), 15);
}
TEST_F(ControlTest, CorruptPrimarySettingsRecoverFromBackupAndDefaultsAreSafe) {
    SettingsStore store(directory);
    EXPECT_FALSE(store.load().at("autostart").get<bool>());
    auto first = defaultSettings();
    first["fps"] = 15;
    store.save(first);
    auto second = first;
    second["fps"] = 60;
    store.save(second);
    std::ofstream(directory / "settings.json") << "{broken";
    EXPECT_EQ(store.load().at("fps"), 15);
    EXPECT_FALSE(store.warning().empty());
    store.save(second);
    EXPECT_EQ(store.load().at("fps"), 60);
    std::filesystem::remove(directory / "settings.json");
    EXPECT_EQ(store.load().at("fps"), 15);
    second["fps"] = 144;
    EXPECT_THROW(store.save(second), std::invalid_argument);
    second = defaultSettings();
    second["pauseBattery"] = "false";
    EXPECT_THROW(store.save(second), std::invalid_argument);
}
TEST_F(ControlTest, UnknownSceneAssignmentPreservesExistingRows) {
    LibraryStore library(directory);
    const auto id = library.importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "stars");
    library.assign({"original"}, id);
    EXPECT_THROW(library.assign({"new"}, "missing"), std::invalid_argument);
    EXPECT_EQ(library.assignments().size(), 1u);
    EXPECT_EQ(library.assignments().at("original"), id);
}
TEST_F(ControlTest, Sha256UsesKnownDigestAndRejectsSizeMismatch) {
    std::filesystem::create_directories(directory);
    const auto file = directory / "bytes.awscene";
    std::ofstream(file, std::ios::binary) << "abc";
    EXPECT_EQ(packageSha256(file, 3),
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_THROW(packageSha256(file, 2), std::invalid_argument);
    EXPECT_THROW(packageSha256(file, 0), std::invalid_argument);
}
TEST_F(ControlTest, DownloadReceiptsNeverSurviveRejectedArchives) {
    ControlService service(directory, runtime());
    const auto file = directory / "broken.awscene";
    std::ofstream(file, std::ios::binary) << "abc";
    Json params{{"path", pathText(file)},
                {"sceneId", "11111111-1111-4111-8111-111111111111"},
                {"sha256", packageSha256(file, 3)},
                {"size", 3},
                {"source", "https://catalog.example/package.awscene"}};
    EXPECT_FALSE(call(service, "importDownloadedScene", params).at("ok").get<bool>());
    params["sha256"] = std::string(64, '0');
    EXPECT_FALSE(call(service, "importDownloadedScene", params).at("ok").get<bool>());
    params["size"] = -1;
    EXPECT_FALSE(call(service, "importDownloadedScene", params).at("ok").get<bool>());
    EXPECT_TRUE(service.library().list().empty());
    EXPECT_TRUE(service.library().downloadHistory().empty());
    EXPECT_FALSE(committed);
    for (const auto& entry : std::filesystem::directory_iterator(directory))
        EXPECT_FALSE(entry.path().filename().string().starts_with(".download-"));
}
TEST_F(ControlTest, MigratesM3SettingsWithoutLosingUserChoices) {
    std::filesystem::create_directories(directory);
    auto legacy = defaultSettings();
    legacy["schemaVersion"] = 1;
    legacy["fps"] = 15;
    legacy["language"] = "en";
    legacy.erase("catalogBaseUrl");
    legacy.erase("catalogAssetOrigins");
    legacy.erase("allowLocalCatalog");
    std::ofstream(directory / "settings.json") << legacy.dump();
    auto migrated = SettingsStore(directory).load();
    EXPECT_EQ(migrated.at("schemaVersion"), 2);
    EXPECT_EQ(migrated.at("fps"), 15);
    EXPECT_EQ(migrated.at("language"), "en");
    EXPECT_EQ(migrated.at("catalogBaseUrl"), "");
    EXPECT_FALSE(migrated.at("allowLocalCatalog").get<bool>());
}
TEST_F(ControlTest, MigratesM3LibraryWithoutLosingFavorites) {
    std::string id;
    {
        LibraryStore library(directory);
        id = library.importScene(std::filesystem::path(AETHERWALL_BUNDLED_DIR) / "stars");
        library.favorite(id, true);
    }
    sqlite3* db = nullptr;
    ASSERT_EQ(sqlite3_open(pathText(directory / "library.sqlite3").c_str(), &db), SQLITE_OK);
    ASSERT_EQ(
        sqlite3_exec(db, "DROP TABLE downloads; PRAGMA user_version=1;", nullptr, nullptr, nullptr),
        SQLITE_OK);
    sqlite3_close(db);
    LibraryStore migrated(directory);
    ASSERT_EQ(migrated.list().size(), 1u);
    EXPECT_EQ(migrated.list()[0]["id"], id);
    EXPECT_EQ(migrated.list()[0]["favorite"], true);
    EXPECT_TRUE(migrated.downloadHistory().empty());
}
TEST(ServiceOptions, ExplicitServiceHasUnlimitedDesktopDefaultsAndCanUseDiagnosticMode) {
    const std::vector<std::wstring> args{L"--service", L"--data-dir", L"my data"};
    const auto options = parseOptions(args);
    EXPECT_EQ(options.durationSeconds, 0);
    EXPECT_EQ(options.mode, RunMode::Desktop);
    EXPECT_EQ(options.dataDirectory, std::filesystem::path(L"my data"));
    const std::vector<std::wstring> diagnostic{L"--service", L"--mode", L"window", L"--duration",
                                               L"12"};
    EXPECT_EQ(parseOptions(diagnostic).mode, RunMode::Window);
    EXPECT_EQ(parseOptions(diagnostic).durationSeconds, 12);
}
} // namespace
} // namespace aetherwall
