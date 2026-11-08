#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "tools/console/BuiltinCommands.hpp"
#include "tools/console/CommandContext.hpp"
#include "tools/console/CommandLine.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/console/ScriptFile.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace {

using tools::Command;
using tools::CommandContext;
using tools::Console;
using tools::CVarFlag;
using tools::CVarManager;
using tools::CVarOptions;
using tools::OutputLevel;
using tools::Statement;

/** @brief A console with the built-in commands and a few cvars. */
class ConsoleTest : public ::testing::Test {
  protected:
    ConsoleTest() {
        tools::registerBuiltinCommands(console);
        cvars.setCheatsAllowed(true);
    }

    /** @brief Text of the last output line. */
    [[nodiscard]] std::string last() const { return console.output().empty() ? "" : console.output().back().text; }

    /** @brief Level of the last output line. */
    [[nodiscard]] OutputLevel lastLevel() const { return console.output().back().level; }

    /** @brief Whether some output line contains a text. */
    [[nodiscard]] bool printed(const std::string& text) const {
        for (const tools::OutputLine& line : console.output()) {
            if (line.text.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    CVarManager cvars;
    Console console{cvars};
    tools::CVarRef<float> gravity =
        cvars.add("sv_gravity", 9.8F, "World gravity", CVarOptions{.flags = CVarFlag::Cheat});
    tools::CVarRef<bool> colliders = cvars.add("r_draw_colliders", false, "Draws the colliders");
    tools::CVarRef<bool> wireframe = cvars.add("r_wireframe", false);
    tools::CVarRef<std::string> playerName = cvars.add("name", std::string("Player"));
};

// --- Line splitting ---------------------------------------------------------------------------------------------

TEST(CommandLine, SplitsWordsStatementsQuotesAndComments) {
    const auto parsed = tools::splitCommandLine(R"(  set name "Big \"Boss\"" ; echo a;;b // ignored)");
    ASSERT_TRUE(parsed);
    const std::vector<Statement> expected{{"set", "name", R"(Big "Boss")"}, {"echo", "a"}, {"b"}};
    EXPECT_EQ(*parsed, expected);
}

TEST(CommandLine, KeepsEmptyQuotedWordsAndQuotedSemicolons) {
    const auto parsed = tools::splitCommandLine(R"(echo "" "a;b" "x // y")");
    ASSERT_TRUE(parsed);
    EXPECT_EQ(*parsed, (std::vector<Statement>{{"echo", "", "a;b", "x // y"}}));
}

TEST(CommandLine, RefusesUntrustedInput) {
    EXPECT_FALSE(tools::splitCommandLine(R"(echo "open)"));
    EXPECT_FALSE(tools::splitCommandLine("echo \x1b[2J"));
    EXPECT_FALSE(tools::splitCommandLine(std::string("echo a\0b", 8)));
    EXPECT_FALSE(tools::splitCommandLine(std::string(tools::MAX_LINE_LENGTH + 1, 'a')));
    std::string many;
    for (std::size_t i = 0; i <= tools::MAX_LINE_WORDS; ++i) {
        many += "w ";
    }
    EXPECT_FALSE(tools::splitCommandLine(many));
    EXPECT_TRUE(tools::splitCommandLine("echo\ta"));
}

TEST(CommandLine, QuoteWordRoundTrips) {
    for (const std::string word : {"plain", "", "two words", R"(q"uote)", R"(back\slash)", "semi;colon", "a//b"}) {
        const auto parsed = tools::splitCommandLine("echo " + tools::quoteWord(word));
        ASSERT_TRUE(parsed) << word;
        ASSERT_EQ(parsed->size(), 1U) << word;
        EXPECT_EQ(parsed->front().at(1), word);
    }
    EXPECT_EQ(tools::quoteWord("plain"), "plain");
}

// --- Execution --------------------------------------------------------------------------------------------------

TEST_F(ConsoleTest, ReadsAndChangesACVarByItsName) {
    EXPECT_TRUE(console.execute("sv_gravity"));
    EXPECT_NE(last().find("sv_gravity = 9.8"), std::string::npos);
    EXPECT_NE(last().find("[cheat]"), std::string::npos);

    EXPECT_TRUE(console.execute("sv_gravity 3.5"));
    EXPECT_FLOAT_EQ(gravity.get(), 3.5F);
    EXPECT_EQ(last(), "sv_gravity = 3.5");
}

TEST_F(ConsoleTest, DescribesEveryFlagOfACVar) {
    (void)cvars.add("sv_version", 3, "Protocol version",
                    CVarOptions{.flags = CVarFlag::Cheat | CVarFlag::Archived | CVarFlag::ReadOnly});
    EXPECT_EQ(tools::describeCVar(cvars, "sv_version"),
              "sv_version = 3 (int, default 3) [cheat] [archived] [read-only] - Protocol version");
    EXPECT_EQ(tools::describeCVar(cvars, "r_wireframe"), "r_wireframe = false (bool, default false)");
    EXPECT_EQ(tools::describeCVar(cvars, "missing"), "");
}

TEST_F(ConsoleTest, SetResetAndToggle) {
    EXPECT_TRUE(console.execute("set name \"Big Boss\""));
    EXPECT_EQ(playerName.get(), "Big Boss");
    EXPECT_EQ(last(), "name = \"Big Boss\"");

    EXPECT_TRUE(console.execute("reset name"));
    EXPECT_EQ(playerName.get(), "Player");

    EXPECT_TRUE(console.execute("toggle r_draw_colliders"));
    EXPECT_TRUE(colliders.get());
    EXPECT_TRUE(console.execute("toggle r_draw_colliders"));
    EXPECT_FALSE(colliders.get());
    EXPECT_FALSE(console.execute("toggle sv_gravity"));
    EXPECT_NE(last().find("not a bool"), std::string::npos);
}

TEST_F(ConsoleTest, RunsEveryStatementOfALine) {
    EXPECT_TRUE(console.execute("r_wireframe 1; r_draw_colliders on"));
    EXPECT_TRUE(wireframe.get());
    EXPECT_TRUE(colliders.get());

    EXPECT_FALSE(console.execute("nope; r_wireframe 0"));  // the second statement still runs
    EXPECT_FALSE(wireframe.get());
}

TEST_F(ConsoleTest, CommandNamesAreCaseInsensitive) {
    EXPECT_TRUE(console.execute("SET r_wireframe true"));
    EXPECT_TRUE(wireframe.get());
}

TEST_F(ConsoleTest, ReportsUnknownNamesBadValuesAndBadUsage) {
    EXPECT_FALSE(console.execute("sv_gravty 1"));
    EXPECT_EQ(lastLevel(), OutputLevel::Error);
    EXPECT_NE(last().find("Unknown command or cvar 'sv_gravty'"), std::string::npos);

    EXPECT_FALSE(console.execute("sv_gravity fast"));
    EXPECT_NE(last().find("expected a number"), std::string::npos);

    EXPECT_FALSE(console.execute("set sv_gravity"));
    EXPECT_EQ(last(), "Usage: set <cvar> <value>");

    EXPECT_FALSE(console.execute("sv_gravity 1 2"));
    EXPECT_NE(last().find("Usage: sv_gravity <value>"), std::string::npos);

    EXPECT_FALSE(console.execute("echo \"open"));
    EXPECT_EQ(last(), "Unterminated quote");
    EXPECT_FLOAT_EQ(gravity.get(), 9.8F);
}

TEST_F(ConsoleTest, CheatCommandsAndCVarsNeedCheats) {
    int spawned = 0;
    console.addCommand("spawn", {.usage = "<prefab>",
                                 .description = "Spawns a prefab",
                                 .handler = [&spawned](CommandContext& /*context*/) { ++spawned; },
                                 .minArgs = 1,
                                 .maxArgs = 1,
                                 .cheat = true});
    cvars.setCheatsAllowed(false);
    EXPECT_FALSE(console.execute("spawn alien"));
    EXPECT_NE(last().find("cheat"), std::string::npos);
    EXPECT_FALSE(console.execute("sv_gravity 1"));
    EXPECT_EQ(spawned, 0);

    cvars.setCheatsAllowed(true);
    EXPECT_TRUE(console.execute("spawn alien"));
    EXPECT_EQ(spawned, 1);
}

TEST_F(ConsoleTest, ACommandThatThrowsIsReportedAsAnError) {
    console.addCommand("boom", {.handler = [](CommandContext& /*context*/) { throw std::runtime_error("kaboom"); }});
    EXPECT_FALSE(console.execute("boom"));
    EXPECT_EQ(last(), "'boom' failed: kaboom");
}

TEST_F(ConsoleTest, CommandsReceiveTheirArguments) {
    std::vector<std::string> received;
    console.addCommand("grab", {.handler = [&received](CommandContext& context) {
                           received.assign(context.args().begin(), context.args().end());
                       }});
    EXPECT_TRUE(console.execute("grab a \"b c\" 3"));
    EXPECT_EQ(received, (std::vector<std::string>{"a", "b c", "3"}));
}

TEST_F(ConsoleTest, ACommandHidesACVarOfTheSameName) {
    bool ran = false;
    console.addCommand("name", {.handler = [&ran](CommandContext& /*context*/) { ran = true; }});
    EXPECT_TRUE(console.execute("name"));
    EXPECT_TRUE(ran);
    EXPECT_TRUE(console.removeCommand("name"));
    EXPECT_FALSE(console.removeCommand("name"));
}

TEST_F(ConsoleTest, RefusesInvalidCommands) {
    EXPECT_THROW(console.addCommand("Bad Name", {.handler = [](CommandContext& /*context*/) {}}),
                 std::invalid_argument);
    EXPECT_THROW(console.addCommand("empty", {}), std::invalid_argument);
}

// --- Built-in commands ------------------------------------------------------------------------------------------

TEST_F(ConsoleTest, HelpListsCommandsAndDescribesOne) {
    EXPECT_TRUE(console.execute("help"));
    EXPECT_TRUE(printed("set <cvar> <value> - Changes a cvar"));
    EXPECT_TRUE(printed("cvar_list [prefix]"));

    EXPECT_TRUE(console.execute("help sv_gravity"));
    EXPECT_NE(last().find("World gravity"), std::string::npos);
    EXPECT_FALSE(console.execute("help nothing"));
}

TEST_F(ConsoleTest, CVarListFiltersByPrefix) {
    EXPECT_TRUE(console.execute("cvar_list r_"));
    EXPECT_TRUE(printed("r_draw_colliders = false"));
    EXPECT_TRUE(printed("r_wireframe = false"));
    EXPECT_FALSE(printed("sv_gravity = "));
    EXPECT_EQ(last(), "2 cvar(s)");
}

TEST_F(ConsoleTest, EchoAndClear) {
    EXPECT_TRUE(console.execute("echo hello   world"));
    EXPECT_EQ(last(), "hello world");
    EXPECT_TRUE(console.execute("clear"));
    EXPECT_TRUE(console.output().empty());
}

// --- Output and history -----------------------------------------------------------------------------------------

TEST_F(ConsoleTest, EchoesTypedLinesAndKeepsAHistory) {
    EXPECT_TRUE(console.execute("echo a"));
    EXPECT_TRUE(console.execute("echo a"));
    EXPECT_TRUE(console.execute("   "));
    EXPECT_FALSE(console.execute("nope"));
    EXPECT_EQ(console.output().front().level, OutputLevel::Echo);
    EXPECT_EQ(console.output().front().text, "> echo a");
    EXPECT_EQ(console.history(), (std::deque<std::string>{"echo a", "nope"}));
}

TEST_F(ConsoleTest, BoundsTheOutputAndTheHistory) {
    for (std::size_t i = 0; i < Console::MAX_HISTORY + 10; ++i) {
        (void)console.execute("echo " + std::to_string(i));
    }
    EXPECT_EQ(console.history().size(), Console::MAX_HISTORY);
    EXPECT_EQ(console.history().back(), "echo " + std::to_string(Console::MAX_HISTORY + 9));
    EXPECT_EQ(console.history().front(), "echo 10");

    for (std::size_t i = 0; i < Console::MAX_OUTPUT_LINES; ++i) {
        console.print(OutputLevel::Info, "line");
    }
    EXPECT_EQ(console.output().size(), Console::MAX_OUTPUT_LINES);
}

TEST_F(ConsoleTest, InvalidLinesAreShownSafelyAndNotRemembered) {
    EXPECT_FALSE(console.execute("echo \x1b[2J"));
    EXPECT_EQ(console.output().front().text, "> echo ?[2J");
    EXPECT_TRUE(console.history().empty());
}

TEST_F(ConsoleTest, PrintSplitsLines) {
    console.print(OutputLevel::Info, "one\r\ntwo");
    ASSERT_EQ(console.output().size(), 2U);
    EXPECT_EQ(console.output()[0].text, "one");
    EXPECT_EQ(console.output()[1].text, "two");
}

// --- Completion -------------------------------------------------------------------------------------------------

TEST_F(ConsoleTest, CompletesAUniqueNameWithATrailingSpace) {
    const tools::Completion completion = console.complete("sv_g");
    EXPECT_EQ(completion.line, "sv_gravity ");
    EXPECT_EQ(completion.candidates, std::vector<std::string>{"sv_gravity"});
    EXPECT_EQ(console.complete("cvar_l").line, "cvar_list ");
    EXPECT_EQ(console.complete("SE").line, "set ");
}

TEST_F(ConsoleTest, CompletesTheSharedPrefixAndListsTheCandidates) {
    const tools::Completion completion = console.complete("r_");
    EXPECT_EQ(completion.line, "r_");
    EXPECT_EQ(completion.candidates, (std::vector<std::string>{"r_draw_colliders", "r_wireframe"}));

    const tools::Completion commands = console.complete("re");
    EXPECT_EQ(commands.line, "reset ");
}

TEST_F(ConsoleTest, CompletesCommandArgumentsAndBoolValues) {
    EXPECT_EQ(console.complete("set sv_").line, "set sv_gravity ");
    EXPECT_EQ(console.complete("toggle r_w").line, "toggle r_wireframe ");
    EXPECT_EQ(console.complete("help cv").line, "help cvar_list ");
    EXPECT_EQ(console.complete("r_wireframe t").line, "r_wireframe true ");
    EXPECT_EQ(console.complete("echo a; set r_d").line, "echo a; set r_draw_colliders ");

    const tools::Completion all = console.complete("set ");
    EXPECT_EQ(all.candidates.size(), cvars.size());
    EXPECT_EQ(all.line, "set ");
}

TEST_F(ConsoleTest, LeavesTheLineAloneWithoutCandidates) {
    EXPECT_EQ(console.complete("zzz").line, "zzz");
    EXPECT_TRUE(console.complete("zzz").candidates.empty());
    EXPECT_EQ(console.complete("set sv_gravity 1").line, "set sv_gravity 1");
    EXPECT_EQ(console.complete("echo \"r_").line, "echo \"r_");
    EXPECT_EQ(console.complete("").candidates.size(), console.commands().size() + cvars.size());
}

// --- Raw commands and quiet runs --------------------------------------------------------------------------------

TEST_F(ConsoleTest, ARawCommandGetsTheRestOfTheLineUntouched) {
    std::vector<std::string> received;
    console.addCommand("code", {
                                   .handler =
                                       [&received](CommandContext& context) {
                                           received.assign(context.args().begin(), context.args().end());
                                       },
                                   .raw = true,
                               });
    EXPECT_TRUE(console.execute(R"(  CODE   System.print("a; b") // kept  )"));
    EXPECT_EQ(received, std::vector<std::string>{R"(System.print("a; b") // kept)"});

    EXPECT_TRUE(console.execute("code"));
    EXPECT_TRUE(received.empty());

    // Raw only when the command starts the line.
    EXPECT_TRUE(console.execute("echo x; code a \"b c\""));
    EXPECT_EQ(received, (std::vector<std::string>{"a", "b c"}));
}

TEST_F(ConsoleTest, RawLinesStillRespectTheLineLimits) {
    console.addCommand("code", {.handler = [](CommandContext& /*context*/) {}, .raw = true});
    EXPECT_FALSE(console.execute("code \x1b[2J"));
    EXPECT_FALSE(console.execute("code " + std::string(tools::MAX_LINE_LENGTH, 'a')));
}

TEST_F(ConsoleTest, RunNeitherEchoesNorRemembers) {
    EXPECT_TRUE(console.run("echo quiet"));
    ASSERT_EQ(console.output().size(), 1U);
    EXPECT_EQ(last(), "quiet");
    EXPECT_TRUE(console.history().empty());
    EXPECT_FALSE(console.run("nope"));
}

// --- exec -------------------------------------------------------------------------------------------------------

/** @brief Fresh script directory under the system temporary directory, removed afterwards. */
class ScriptDir {
  public:
    ScriptDir() {
        static int counter = 0;
        _path = std::filesystem::temp_directory_path() /
                ("rtype_console_scripts_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
                 std::to_string(++counter));
        std::filesystem::create_directories(_path);
    }
    ~ScriptDir() {
        std::error_code error;
        std::filesystem::remove_all(_path, error);
    }
    ScriptDir(const ScriptDir&) = delete;
    ScriptDir& operator=(const ScriptDir&) = delete;
    ScriptDir(ScriptDir&&) = delete;
    ScriptDir& operator=(ScriptDir&&) = delete;

    void write(const std::string& name, std::string_view content) const {
        std::filesystem::create_directories((_path / name).parent_path());
        std::ofstream(_path / name, std::ios::binary) << content;
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }

  private:
    std::filesystem::path _path;
};

TEST_F(ConsoleTest, ExecRunsAFileOfConsoleLines) {
    const ScriptDir dir;
    dir.write("debug.cfg", "// debug views\r\nr_wireframe 1\n\nr_draw_colliders on; echo done\n");
    console.setScriptRoot(dir.path());

    EXPECT_TRUE(console.execute("exec debug.cfg"));
    EXPECT_TRUE(wireframe.get());
    EXPECT_TRUE(colliders.get());
    EXPECT_EQ(last(), "done");
    EXPECT_EQ(console.history(), std::deque<std::string>{"exec debug.cfg"});  // the lines of the file are not
}

TEST_F(ConsoleTest, ExecReportsTheFailingLines) {
    const ScriptDir dir;
    dir.write("broken.cfg", "nope\nr_wireframe 1\nsv_gravity fast\n");
    console.setScriptRoot(dir.path());

    EXPECT_FALSE(console.execute("exec broken.cfg"));
    EXPECT_TRUE(wireframe.get());  // the other lines still run
    EXPECT_EQ(last(), "'broken.cfg': 2 line(s) failed");
}

TEST_F(ConsoleTest, ExecHandsFilesToTheRunnerOfTheirExtension) {
    const ScriptDir dir;
    dir.write("scripts/boss.LUA", "spawn boss");
    console.setScriptRoot(dir.path());
    std::string ran;
    console.addScriptRunner(".lua",
                            [&ran](CommandContext& /*context*/, std::string_view source, std::string_view path) {
                                ran = std::string(path) + ": " + std::string(source);
                            });

    EXPECT_TRUE(console.execute("exec scripts/boss.LUA"));
    EXPECT_EQ(ran, "scripts/boss.LUA: spawn boss");

    EXPECT_TRUE(console.removeScriptRunner(".LUA"));
    EXPECT_FALSE(console.execute("exec scripts/boss.LUA"));  // back to console lines: no `spawn` command
}

TEST_F(ConsoleTest, ExecStaysInsideTheScriptDirectory) {
    const ScriptDir dir;
    dir.write("inside/ok.cfg", "echo ok");
    dir.write("secret.cfg", "echo secret");
    console.setScriptRoot(dir.path() / "inside");

    EXPECT_TRUE(console.execute("exec ok.cfg"));
    EXPECT_FALSE(console.execute("exec ../secret.cfg"));
    EXPECT_NE(last().find("outside the script directory"), std::string::npos);
    EXPECT_FALSE(console.execute("exec " + (dir.path() / "secret.cfg").string()));
    EXPECT_NE(last().find("relative"), std::string::npos);
    EXPECT_FALSE(console.execute("exec missing.cfg"));
    EXPECT_NE(last().find("no such script file"), std::string::npos);
    EXPECT_FALSE(console.execute("exec ."));
}

TEST_F(ConsoleTest, ExecRefusesFilesTooLarge) {
    const ScriptDir dir;
    dir.write("huge.cfg", std::string(tools::MAX_SCRIPT_SIZE + 1, '\n'));
    console.setScriptRoot(dir.path());
    EXPECT_FALSE(console.execute("exec huge.cfg"));
    EXPECT_NE(last().find("larger than"), std::string::npos);
}

TEST_F(ConsoleTest, ExecStopsAScriptThatRunsItself) {
    const ScriptDir dir;
    dir.write("loop.cfg", "exec loop.cfg");
    console.setScriptRoot(dir.path());
    EXPECT_FALSE(console.execute("exec loop.cfg"));
    EXPECT_TRUE(printed("Scripts nested too deep"));
}

TEST_F(ConsoleTest, ScriptRunnersNeedAnExtensionAndAFunction) {
    EXPECT_THROW(console.addScriptRunner("lua", [](CommandContext&, std::string_view, std::string_view) {}),
                 std::invalid_argument);
    EXPECT_THROW(console.addScriptRunner(".lua", {}), std::invalid_argument);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
