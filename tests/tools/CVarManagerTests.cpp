#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsoleEvents.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace {

using tools::CVarFlag;
using tools::CVarManager;
using tools::CVarOptions;
using tools::CVarRange;

[[nodiscard]] std::vector<std::string> drain(CVarManager& cvars) {
    std::vector<std::string> names;
    cvars.drainChanged([&names](std::string_view name) { names.emplace_back(name); });
    return names;
}

// --- Registration -----------------------------------------------------------------------------------------------

TEST(CVarManager, RegistersEachTypeAtItsDefault) {
    CVarManager cvars;
    const auto flag = cvars.add("r_wireframe", false, "Draws wireframes");
    const auto lag = cvars.add("net_sim_lag", 0, "Added latency (ms)");
    const auto gravity = cvars.add("sv_gravity", 9.8F, "World gravity");
    const auto player = cvars.add("name", std::string("Player"));

    EXPECT_FALSE(flag.get());
    EXPECT_EQ(lag.get(), 0);
    EXPECT_FLOAT_EQ(gravity.get(), 9.8F);
    EXPECT_EQ(player.get(), "Player");
    EXPECT_EQ(cvars.size(), 4U);
    ASSERT_NE(cvars.find("sv_gravity"), nullptr);
    EXPECT_EQ(cvars.find("sv_gravity")->typeName(), "float");
    EXPECT_EQ(cvars.find("sv_gravity")->description(), "World gravity");
    EXPECT_EQ(cvars.find("missing"), nullptr);
}

TEST(CVarManager, RegisteringAgainWithTheSameTypeReturnsTheExistingVariable) {
    CVarManager cvars;
    const auto first = cvars.add("sv_gravity", 9.8F);
    ASSERT_TRUE(first.set(3.0F));

    const auto second = cvars.add("sv_gravity", 1.0F);
    EXPECT_EQ(&first.cvar(), &second.cvar());
    EXPECT_FLOAT_EQ(second.get(), 3.0F);
    EXPECT_EQ(cvars.size(), 1U);
}

TEST(CVarManager, RefusesATypeConflictAnInvalidNameAndAnOutOfRangeDefault) {
    CVarManager cvars;
    (void)cvars.add("sv_gravity", 9.8F);
    EXPECT_THROW((void)cvars.add("sv_gravity", 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add("", 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add("Upper", 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add("9lives", 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add("with space", 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add(std::string(CVarManager::MAX_NAME_LENGTH + 1, 'a'), 1), std::invalid_argument);
    EXPECT_THROW((void)cvars.add("speed", 50, "", CVarOptions{.range = CVarRange{.min = 0, .max = 10}}),
                 std::invalid_argument);
    EXPECT_THROW((void)cvars.add("motd", std::string("line\nbreak")), std::invalid_argument);
}

TEST(CVarManager, AcceptsDottedAndUnderscoredNames) {
    EXPECT_TRUE(CVarManager::isValidName("r_draw_colliders"));
    EXPECT_TRUE(CVarManager::isValidName("_private"));
    EXPECT_TRUE(CVarManager::isValidName("audio.master2"));
    EXPECT_FALSE(CVarManager::isValidName("a-b"));
    EXPECT_FALSE(CVarManager::isValidName(".dot"));
}

TEST(CVarManager, RefGivesATypedHandleOnlyForTheRightType) {
    CVarManager cvars;
    (void)cvars.add("sv_gravity", 9.8F);
    EXPECT_TRUE(cvars.ref<float>("sv_gravity").has_value());
    EXPECT_FALSE(cvars.ref<int>("sv_gravity").has_value());
    EXPECT_FALSE(cvars.ref<float>("missing").has_value());
}

// --- Changes from text ------------------------------------------------------------------------------------------

TEST(CVarManager, ParsesBooleansIntegersFloatsAndStrings) {
    CVarManager cvars;
    const auto flag = cvars.add("flag", false);
    const auto count = cvars.add("count", 0);
    const auto speed = cvars.add("speed", 0.0F);
    const auto text = cvars.add("text", std::string());

    for (const char* yes : {"1", "true", "TRUE", "on", "yes"}) {
        ASSERT_TRUE(cvars.setFromString("flag", "0"));
        ASSERT_TRUE(cvars.setFromString("flag", yes)) << yes;
        EXPECT_TRUE(flag.get()) << yes;
    }
    ASSERT_TRUE(cvars.setFromString("count", "-42"));
    EXPECT_EQ(count.get(), -42);
    ASSERT_TRUE(cvars.setFromString("count", "+7"));
    EXPECT_EQ(count.get(), 7);
    ASSERT_TRUE(cvars.setFromString("speed", "12.5"));
    EXPECT_FLOAT_EQ(speed.get(), 12.5F);
    ASSERT_TRUE(cvars.setFromString("speed", "1e2"));
    EXPECT_FLOAT_EQ(speed.get(), 100.0F);
    ASSERT_TRUE(cvars.setFromString("text", "hello world"));
    EXPECT_EQ(text.get(), "hello world");
}

TEST(CVarManager, RefusesMalformedValuesAndKeepsTheOldOne) {
    CVarManager cvars;
    const auto flag = cvars.add("flag", true);
    const auto count = cvars.add("count", 3);
    const auto speed = cvars.add("speed", 1.0F);

    for (const char* bad : {"", "maybe", "2"}) {
        EXPECT_FALSE(cvars.setFromString("flag", bad)) << bad;
    }
    for (const char* bad : {"", "abc", "1.5", "12abc", "99999999999999999999", "+", "0x10"}) {
        EXPECT_FALSE(cvars.setFromString("count", bad)) << bad;
    }
    for (const char* bad : {"", "fast", "1.0f", "nan", "inf", "1e999"}) {
        EXPECT_FALSE(cvars.setFromString("speed", bad)) << bad;
    }
    EXPECT_TRUE(flag.get());
    EXPECT_EQ(count.get(), 3);
    EXPECT_FLOAT_EQ(speed.get(), 1.0F);
    EXPECT_TRUE(drain(cvars).empty());
}

TEST(CVarManager, RefusesValuesOutOfRange) {
    CVarManager cvars;
    const auto lag = cvars.add("net_sim_lag", 0, "", CVarOptions{.range = CVarRange{.min = 0, .max = 1000}});
    EXPECT_TRUE(cvars.setFromString("net_sim_lag", "1000"));
    const auto refused = cvars.setFromString("net_sim_lag", "-1");
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.error().find("range"), std::string::npos);
    EXPECT_FALSE(lag.set(5000));
    EXPECT_EQ(lag.get(), 1000);
}

TEST(CVarManager, RefusesStringsTooLongOrWithControlCharacters) {
    CVarManager cvars;
    const auto text = cvars.add("text", std::string("safe"));
    EXPECT_FALSE(cvars.setFromString("text", std::string(CVarManager::MAX_STRING_LENGTH + 1, 'x')));
    EXPECT_TRUE(cvars.setFromString("text", std::string(CVarManager::MAX_STRING_LENGTH, 'x')));
    EXPECT_FALSE(cvars.setFromString("text", "bell\a"));
    EXPECT_FALSE(cvars.setFromString("text", "esc\x1b[2J"));
    EXPECT_FALSE(text.set("tab\there"));
    EXPECT_EQ(text.get(), std::string(CVarManager::MAX_STRING_LENGTH, 'x'));
}

TEST(CVarManager, UnknownVariablesGiveAClearError) {
    CVarManager cvars;
    const auto refused = cvars.setFromString("sv_gravty", "1");
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), "Unknown cvar 'sv_gravty'");
    EXPECT_FALSE(cvars.reset("sv_gravty"));
}

TEST(CVarManager, ResetRestoresTheDefault) {
    CVarManager cvars;
    const auto gravity = cvars.add("sv_gravity", 9.8F);
    ASSERT_TRUE(cvars.setFromString("sv_gravity", "1"));
    ASSERT_TRUE(cvars.reset("sv_gravity"));
    EXPECT_FLOAT_EQ(gravity.get(), 9.8F);
}

// --- Flags ------------------------------------------------------------------------------------------------------

TEST(CVarManager, ReadOnlyVariablesAreChangedByCodeOnly) {
    CVarManager cvars;
    const auto version = cvars.add("sv_version", 1, "", CVarOptions{.flags = CVarFlag::ReadOnly});
    const auto refused = cvars.setFromString("sv_version", "2");
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.error().find("read-only"), std::string::npos);
    EXPECT_FALSE(cvars.reset("sv_version"));
    EXPECT_TRUE(version.set(2));
    EXPECT_EQ(version.get(), 2);
}

TEST(CVarManager, CheatVariablesNeedCheatsAndAreResetWhenTheyGetDisabled) {
    CVarManager cvars;
    EXPECT_FALSE(cvars.cheatsAllowed());
    const auto gravity = cvars.add("sv_gravity", 9.8F, "", CVarOptions{.flags = CVarFlag::Cheat | CVarFlag::Archived});
    EXPECT_TRUE(tools::hasFlag(gravity.cvar().flags(), CVarFlag::Archived));

    const auto refused = cvars.setFromString("sv_gravity", "1");
    ASSERT_FALSE(refused);
    EXPECT_NE(refused.error().find("cheat"), std::string::npos);

    cvars.setCheatsAllowed(true);
    ASSERT_TRUE(cvars.setFromString("sv_gravity", "1"));
    EXPECT_FLOAT_EQ(gravity.get(), 1.0F);
    (void)drain(cvars);

    cvars.setCheatsAllowed(false);
    EXPECT_FLOAT_EQ(gravity.get(), 9.8F);
    EXPECT_EQ(drain(cvars), std::vector<std::string>{"sv_gravity"});
}

// --- Change tracking and listing --------------------------------------------------------------------------------

TEST(CVarManager, RecordsEachActualChangeOnce) {
    CVarManager cvars;
    const auto a = cvars.add("a", 0);
    const auto b = cvars.add("b", false);
    ASSERT_TRUE(cvars.setFromString("b", "true"));
    ASSERT_TRUE(cvars.setFromString("a", "1"));
    ASSERT_TRUE(cvars.setFromString("b", "false"));
    ASSERT_TRUE(a.set(1));  // same value: not a change
    EXPECT_EQ(drain(cvars), (std::vector<std::string>{"b", "a"}));
    EXPECT_TRUE(drain(cvars).empty());
    (void)b;
}

TEST(CVarManager, ListsByPrefixInNameOrder) {
    CVarManager cvars;
    (void)cvars.add("sv_gravity", 9.8F);
    (void)cvars.add("r_wireframe", false);
    (void)cvars.add("r_draw_colliders", false);

    std::vector<std::string> names;
    for (const tools::CVar* cvar : cvars.list("r_")) {
        names.push_back(cvar->name());
    }
    EXPECT_EQ(names, (std::vector<std::string>{"r_draw_colliders", "r_wireframe"}));
    EXPECT_EQ(cvars.list().size(), 3U);
    EXPECT_TRUE(cvars.list("zz").empty());
}

TEST(CVarManager, FormatsValuesAsTheConsoleShowsThem) {
    EXPECT_EQ(CVarManager::format(true), "true");
    EXPECT_EQ(CVarManager::format(-3), "-3");
    EXPECT_EQ(CVarManager::format(9.8F), "9.8");
    EXPECT_EQ(CVarManager::format(std::string("hi")), "hi");
}

// --- In the engine ----------------------------------------------------------------------------------------------

TEST(ConsolePlugin, AConsoleCommandChangesTheValueReadByASystemImmediately) {
    engine::Engine engine;
    engine.addPlugin<tools::ConsolePlugin>(tools::ConsoleConfig{.allowCheats = true});
    std::vector<std::string> events;
    (void)engine.eventBus().subscribe<tools::CVarChanged>(
        [&events](const tools::CVarChanged& e) { events.push_back(e.name); });
    engine.init();

    auto& cvars = engine.service<CVarManager>();
    auto& console = engine.service<tools::Console>();
    EXPECT_EQ(&cvars, &engine.getPlugin<tools::ConsolePlugin>().cvars());
    const auto gravity = cvars.add("sv_gravity", 9.8F, "World gravity", CVarOptions{.flags = CVarFlag::Cheat});

    EXPECT_TRUE(console.execute("set sv_gravity 12"));
    EXPECT_FLOAT_EQ(gravity.get(), 12.0F);  // what a physics system reads at its next step
    EXPECT_TRUE(events.empty());

    engine.getPlugin<tools::ConsolePlugin>().preUpdate(engine);
    EXPECT_EQ(events, std::vector<std::string>{"sv_gravity"});

    engine.shutdown();
    EXPECT_FALSE(engine.mediator().hasService<CVarManager>());
    EXPECT_FALSE(engine.mediator().hasService<tools::Console>());
}

TEST(ConsolePlugin, DisallowsCheatsByDefault) {
    tools::ConsolePlugin plugin;
    const auto gravity = plugin.cvars().add("sv_gravity", 9.8F, "", CVarOptions{.flags = CVarFlag::Cheat});
    EXPECT_FALSE(plugin.console().execute("sv_gravity 0"));
    EXPECT_FLOAT_EQ(gravity.get(), 9.8F);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
