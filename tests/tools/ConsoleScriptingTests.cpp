#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <system_error>

#include "core/Engine.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/WrenVM.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/scripting/ConsoleScriptingPlugin.hpp"

namespace {

/** @brief Fresh script directory under the system temporary directory, removed afterwards. */
class ScriptDir {
  public:
    ScriptDir() {
        static int counter = 0;
        _path = std::filesystem::temp_directory_path() /
                ("rtype_console_wren_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" +
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
        std::ofstream(_path / name, std::ios::binary) << content;
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return _path; }

  private:
    std::filesystem::path _path;
};

/** @brief An engine with the console, the script plugin and the bridge. */
class ConsoleScriptingTest : public ::testing::Test {
  protected:
    explicit ConsoleScriptingTest(bool allowCheats = true) {
        engine.addPlugin<tools::ConsolePlugin>(
            tools::ConsoleConfig{.allowCheats = allowCheats, .scriptRoot = scripts.path()});
        engine.addPlugin<scripting::ScriptPlugin>();
        engine.addPlugin<tools::ConsoleScriptingPlugin>();
        engine.init();
    }
    ~ConsoleScriptingTest() override { engine.shutdown(); }
    ConsoleScriptingTest(const ConsoleScriptingTest&) = delete;
    ConsoleScriptingTest& operator=(const ConsoleScriptingTest&) = delete;
    ConsoleScriptingTest(ConsoleScriptingTest&&) = delete;
    ConsoleScriptingTest& operator=(ConsoleScriptingTest&&) = delete;

    [[nodiscard]] tools::Console& console() { return engine.service<tools::Console>(); }

    [[nodiscard]] scripting::WrenVM& vm() { return engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().getVm(); }

    [[nodiscard]] std::string last() { return console().output().back().text; }

    [[nodiscard]] tools::OutputLevel lastLevel() { return console().output().back().level; }

    /** @brief Whether some output line contains a text. */
    [[nodiscard]] bool printed(std::string_view text) {
        for (const tools::OutputLine& line : console().output()) {
            if (line.text.find(text) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    ScriptDir scripts;
    engine::Engine engine;
};

class ConsoleScriptingWithoutCheatsTest : public ConsoleScriptingTest {
  protected:
    ConsoleScriptingWithoutCheatsTest() : ConsoleScriptingTest(false) {}
};

// --- wren <code> ------------------------------------------------------------------------------------------------

TEST_F(ConsoleScriptingTest, AnExpressionPrintsItsValue) {
    EXPECT_TRUE(console().execute("wren 1 + 2"));
    EXPECT_EQ(last(), "=> 3");
    EXPECT_TRUE(console().execute(R"wren(wren "ab" * 2)wren"));
    EXPECT_EQ(last(), "=> abab");
}

TEST_F(ConsoleScriptingTest, ANullValuePrintsNothing) {
    const std::size_t before = console().output().size();
    EXPECT_TRUE(console().execute("wren null"));
    EXPECT_EQ(console().output().size(), before + 1);  // the echoed line only
}

TEST_F(ConsoleScriptingTest, TopLevelVariablesStayDefinedBetweenLines) {
    EXPECT_TRUE(console().execute("wren var speed = 3"));
    EXPECT_TRUE(console().execute("wren speed * 2"));
    EXPECT_EQ(last(), "=> 6");
    EXPECT_TRUE(console().execute("wren speed = 10"));
    EXPECT_TRUE(console().execute("wren speed"));
    EXPECT_EQ(last(), "=> 10");
}

TEST_F(ConsoleScriptingTest, SystemPrintGoesToTheConsole) {
    EXPECT_TRUE(console().execute(R"wren(wren for (i in 1..2) System.print("line %(i)"))wren"));
    EXPECT_TRUE(printed("line 1"));
    EXPECT_EQ(last(), "line 2");
}

TEST_F(ConsoleScriptingTest, TheCodeReachesWrenUntouched) {
    EXPECT_TRUE(console().execute(R"wren(wren var text = "a; b" // a comment)wren"));
    EXPECT_TRUE(console().execute("wren text"));
    EXPECT_EQ(last(), "=> a; b");
}

TEST_F(ConsoleScriptingTest, CompileErrorsAreReported) {
    EXPECT_FALSE(console().execute("wren var = 3"));
    EXPECT_EQ(lastLevel(), tools::OutputLevel::Error);
    EXPECT_TRUE(printed("console"));
}

TEST_F(ConsoleScriptingTest, ARuntimeErrorDoesNotRunTheCodeTwice) {
    EXPECT_TRUE(console().execute("wren var count = 0"));
    EXPECT_FALSE(console().execute(R"wren(wren [count = count + 1, Fiber.abort("boom")])wren"));
    EXPECT_TRUE(printed("boom"));
    EXPECT_TRUE(console().execute("wren count"));
    EXPECT_EQ(last(), "=> 1");
}

TEST_F(ConsoleScriptingTest, TheVmHandlersAreRestoredAfterARun) {
    EXPECT_TRUE(console().execute("wren 1"));
    EXPECT_FALSE(console().execute("wren var = 1"));
    EXPECT_FALSE(vm().writeHandler());
    EXPECT_FALSE(vm().errorHandler());
}

TEST_F(ConsoleScriptingTest, TheCommandNeedsCode) {
    EXPECT_FALSE(console().execute("wren"));
    EXPECT_EQ(last(), "Usage: wren <code>");
}

TEST_F(ConsoleScriptingWithoutCheatsTest, WrenIsACheat) {
    EXPECT_FALSE(console().execute("wren 1 + 2"));
    EXPECT_NE(last().find("cheat"), std::string::npos);
}

// --- exec <file>.wren -------------------------------------------------------------------------------------------

TEST_F(ConsoleScriptingTest, ExecRunsAWrenFileEachTimeInAModuleOfItsOwn) {
    scripts.write("hello.wren", "var greeting = \"from file\"\nSystem.print(greeting)\n");
    EXPECT_TRUE(console().execute("exec hello.wren"));
    EXPECT_EQ(last(), "from file");
    // A second run defines `greeting` again: it works because each run gets a fresh module.
    EXPECT_TRUE(console().execute("exec hello.wren"));
    EXPECT_EQ(last(), "from file");
}

TEST_F(ConsoleScriptingTest, ExecReportsTheErrorsOfAWrenFile) {
    scripts.write("broken.wren", "System.print(\"before\")\nFiber.abort(\"bad file\")\n");
    EXPECT_FALSE(console().execute("exec broken.wren"));
    EXPECT_TRUE(printed("before"));
    EXPECT_TRUE(printed("bad file"));
}

TEST_F(ConsoleScriptingTest, ACfgFileCanRunWrenLines) {
    scripts.write("setup.cfg", "wren var level = 4\nwren level + 1\n");
    EXPECT_TRUE(console().execute("exec setup.cfg"));
    EXPECT_EQ(last(), "=> 5");
}

TEST_F(ConsoleScriptingWithoutCheatsTest, ExecOfAWrenFileIsACheat) {
    scripts.write("hello.wren", "System.print(\"hi\")\n");
    EXPECT_FALSE(console().execute("exec hello.wren"));
    EXPECT_NE(last().find("cheat"), std::string::npos);
}

TEST_F(ConsoleScriptingTest, ShutdownRemovesTheCommandAndTheRunner) {
    tools::Console& kept = console();
    engine.getPlugin<tools::ConsoleScriptingPlugin>().shutdown(engine);
    EXPECT_EQ(kept.findCommand("wren"), nullptr);
    EXPECT_FALSE(kept.removeScriptRunner(".wren"));
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
