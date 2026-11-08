#include <gtest/gtest.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "config/ConfigFile.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "ecs/time/Time.hpp"
#include "settings/SettingsEvents.hpp"
#include "settings/SettingsPlugin.hpp"

namespace {

/** @brief Fresh directory under the system temporary directory, removed afterwards. */
class TempDir {
  public:
    TempDir() {
        static int counter = 0;
        _path = std::filesystem::temp_directory_path() /
                ("rtype_settings_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                 std::to_string(++counter));
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(_path, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
    TempDir(TempDir&&) = delete;
    TempDir& operator=(TempDir&&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }

  private:
    std::filesystem::path _path;
};

void writeFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path);
    out << text;
}

TEST(ConfigFileWrite, NestedKeysRoundTripThroughToml) {
    config::ConfigFile file;
    file.set("audio.master", 0.5);
    file.set("audio.muted", false);
    file.set("net.last_port", std::int64_t{4242});
    file.set("player.name", std::string("Ripley"));
    file.set("top", true);

    const config::ConfigFile back = config::ConfigFile::parse(file.toToml());
    ASSERT_FALSE(back.error().has_value());
    EXPECT_EQ(back.getNumber("audio.master"), 0.5);
    EXPECT_EQ(back.getBool("audio.muted"), false);
    EXPECT_EQ(back.getInteger("net.last_port"), 4242);
    EXPECT_EQ(back.getString("player.name"), "Ripley");
    EXPECT_EQ(back.getBool("top"), true);

    file.erase("top");
    EXPECT_FALSE(file.contains("top"));
}

TEST(ConfigFileWrite, SaveCreatesDirectoriesAndLeavesNoTemporaryFile) {
    const TempDir dir;
    const std::filesystem::path path = dir.path() / "deep" / "settings.toml";
    config::ConfigFile file;
    file.set("a.b", std::int64_t{1});
    ASSERT_TRUE(file.save(path).has_value());
    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_FALSE(std::filesystem::exists(path.string() + ".tmp"));
    EXPECT_EQ(config::ConfigFile::load(path).getInteger("a.b"), 1);

    file.set("a.b", std::int64_t{2});
    ASSERT_TRUE(file.save(path).has_value());
    EXPECT_EQ(config::ConfigFile::load(path).getInteger("a.b"), 2);
}

TEST(Settings, DefaultsTypesAndChanges) {
    config::Settings settings;
    settings.define("audio.master", 1.0);
    settings.define("debug.metrics", true);
    settings.define("player.name", std::string("Player"));

    EXPECT_EQ(settings.getNumber("audio.master"), 1.0);
    EXPECT_FALSE(settings.dirty());

    EXPECT_TRUE(settings.set("audio.master", std::int64_t{0}));  // integer accepted for a number
    EXPECT_EQ(settings.getNumber("audio.master"), 0.0);
    EXPECT_FALSE(settings.set("audio.master", 0.0));                // unchanged
    EXPECT_FALSE(settings.set("debug.metrics", std::string("x")));  // wrong type
    EXPECT_FALSE(settings.set("missing", true));                    // not declared
    EXPECT_TRUE(settings.set("player.name", std::string("Ripley")));
    EXPECT_TRUE(settings.dirty());

    std::vector<std::string> changed;
    settings.drainChanged([&](std::string_view key) { changed.emplace_back(key); });
    EXPECT_EQ(changed, (std::vector<std::string>{"audio.master", "player.name"}));
    changed.clear();
    settings.drainChanged([&](std::string_view key) { changed.emplace_back(key); });
    EXPECT_TRUE(changed.empty());

    EXPECT_TRUE(settings.reset("player.name"));
    EXPECT_EQ(settings.getString("player.name"), "Player");
    EXPECT_THROW((void)settings.getBool("missing"), std::out_of_range);
    EXPECT_THROW((void)settings.getBool("player.name"), std::out_of_range);
}

TEST(Settings, LoadAppliesValuesWarnsOnWrongTypesAndKeepsUnknownKeys) {
    const TempDir dir;
    const std::filesystem::path path = dir.path() / "settings.toml";
    writeFile(path, "future = \"kept\"\n[audio]\nmaster = 0.25\nmusic = \"loud\"\n");

    config::Settings settings(path);
    settings.define("audio.master", 1.0);
    settings.define("audio.music", 1.0);
    ASSERT_TRUE(settings.load().has_value());
    EXPECT_EQ(settings.getNumber("audio.master"), 0.25);
    EXPECT_EQ(settings.getNumber("audio.music"), 1.0);
    EXPECT_EQ(settings.warnings().size(), 1U);
    EXPECT_FALSE(settings.dirty());

    settings.define("player.name", std::string("Player"));  // declared after the load
    ASSERT_TRUE(settings.set("audio.master", 0.75));
    ASSERT_TRUE(settings.save().has_value());
    EXPECT_FALSE(settings.dirty());

    const config::ConfigFile saved = config::ConfigFile::load(path);
    EXPECT_EQ(saved.getString("future"), "kept");
    EXPECT_EQ(saved.getNumber("audio.master"), 0.75);
    EXPECT_EQ(saved.getString("player.name"), "Player");
}

TEST(Settings, DeclaringAfterLoadUsesTheLoadedValue) {
    const TempDir dir;
    const std::filesystem::path path = dir.path() / "settings.toml";
    writeFile(path, "[net]\nlast_host = \"10.0.0.2\"\n");
    config::Settings settings(path);
    ASSERT_TRUE(settings.load().has_value());
    settings.define("net.last_host", std::string("127.0.0.1"));
    EXPECT_EQ(settings.getString("net.last_host"), "10.0.0.2");
}

TEST(Settings, MissingFileIsAFirstLaunchAndCorruptFileKeepsDefaults) {
    const TempDir dir;
    config::Settings fresh(dir.path() / "none.toml");
    fresh.define("a", true);
    EXPECT_TRUE(fresh.load().has_value());
    EXPECT_TRUE(fresh.getBool("a"));

    const std::filesystem::path path = dir.path() / "broken.toml";
    writeFile(path, "this is [ not toml");
    config::Settings broken(path);
    broken.define("a", true);
    EXPECT_FALSE(broken.load().has_value());
    EXPECT_TRUE(broken.getBool("a"));
}

#if !defined(_WIN32) && !defined(__APPLE__)
TEST(Settings, UserConfigDirectoryFollowsXdg) {
    const char* previous = std::getenv("XDG_CONFIG_HOME");
    const std::string saved = previous != nullptr ? previous : "";
    ::setenv("XDG_CONFIG_HOME", "/tmp/xdg-test", 1);
    EXPECT_EQ(config::userConfigDirectory("rtype"), std::filesystem::path("/tmp/xdg-test/rtype"));
    if (previous != nullptr) {
        ::setenv("XDG_CONFIG_HOME", saved.c_str(), 1);
    } else {
        ::unsetenv("XDG_CONFIG_HOME");
    }
}
#endif

TEST(SettingsPlugin, PublishesChangesAndSavesAfterTheQuietTime) {
    const TempDir dir;
    const std::filesystem::path path = dir.path() / "settings.toml";
    writeFile(path, "[audio]\nmaster = 0.5\n");

    engine::Engine engine;
    engine.addPlugin<settings::SettingsPlugin>(path, 1.0F);
    auto& plugin = engine.getPlugin<settings::SettingsPlugin>();
    plugin.settings().define("audio.master", 1.0);
    std::vector<std::string> events;
    (void)engine.eventBus().subscribe<settings::SettingChanged>(
        [&](const settings::SettingChanged& e) { events.push_back(e.key); });
    engine.init();

    EXPECT_EQ(&engine.service<config::Settings>(), &plugin.settings());
    plugin.update(engine, ecs::time::Time{.deltaTime = 0.1F});
    EXPECT_EQ(events, (std::vector<std::string>{"audio.master"}));  // the loaded value differs from the default

    ASSERT_TRUE(plugin.settings().set("audio.master", 0.2));
    plugin.update(engine, ecs::time::Time{.deltaTime = 0.4F});
    plugin.update(engine, ecs::time::Time{.deltaTime = 0.4F});
    EXPECT_EQ(config::ConfigFile::load(path).getNumber("audio.master"), 0.5);  // not quiet long enough yet
    plugin.update(engine, ecs::time::Time{.deltaTime = 0.4F});
    EXPECT_EQ(config::ConfigFile::load(path).getNumber("audio.master"), 0.2);
    EXPECT_FALSE(plugin.settings().dirty());

    ASSERT_TRUE(plugin.settings().set("audio.master", 0.3));
    engine.shutdown();
    EXPECT_EQ(config::ConfigFile::load(path).getNumber("audio.master"), 0.3);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
