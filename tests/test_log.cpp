#include <gtest/gtest.h>

#include <string>

#include "Log/LogStyle.hpp"
#include "Log/Logger.hpp"

namespace {

class LoggerTest : public ::testing::Test {
  protected:
    void TearDown() override {
        rtype::log::setVerbose(false);
        rtype::log::setColored(false);
    }
};

}  // namespace

TEST_F(LoggerTest, VerboseIsOffByDefault) { EXPECT_FALSE(rtype::log::isVerbose()); }

TEST_F(LoggerTest, DebugIsHiddenWithoutVerbose) {
    testing::internal::CaptureStdout();
    rtype::log::debug("test", "hidden ", 42);
    EXPECT_TRUE(testing::internal::GetCapturedStdout().empty());
}

TEST_F(LoggerTest, DebugIsPrintedWithVerbose) {
    rtype::log::setVerbose(true);
    testing::internal::CaptureStdout();
    rtype::log::debug("test", "client ", 7, " joined");
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("DEBUG [test] client 7 joined\n"), std::string::npos);
}

TEST_F(LoggerTest, InfoGoesToStdout) {
    testing::internal::CaptureStdout();
    rtype::log::info("server", "listening on port ", 4242);
    const std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("INFO  [server] listening on port 4242\n"), std::string::npos);
}

TEST_F(LoggerTest, WarningsAndErrorsGoToStderr) {
    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    rtype::log::warning("network", "server full");
    rtype::log::error("network", "cannot start");
    const std::string out = testing::internal::GetCapturedStdout();
    const std::string err = testing::internal::GetCapturedStderr();
    EXPECT_TRUE(out.empty());
    EXPECT_NE(err.find("WARN  [network] server full\n"), std::string::npos);
    EXPECT_NE(err.find("ERROR [network] cannot start\n"), std::string::npos);
}

TEST_F(LoggerTest, LinesStartWithATimestamp) {
    testing::internal::CaptureStdout();
    rtype::log::info("test", "x");
    const std::string output = testing::internal::GetCapturedStdout();
    ASSERT_FALSE(output.empty());
    EXPECT_EQ(output.front(), '[');
    EXPECT_NE(output.find("] INFO "), std::string::npos);
}

TEST_F(LoggerTest, NoAnsiCodesByDefault) {
    testing::internal::CaptureStdout();
    rtype::log::info("server", "plain");
    EXPECT_EQ(testing::internal::GetCapturedStdout().find('\x1b'), std::string::npos);
}

TEST_F(LoggerTest, ColoredLinesUseAnsiCodesAndKeepTheText) {
    rtype::log::setColored(true);
    testing::internal::CaptureStderr();
    rtype::log::error("network", "cannot start");
    const std::string output = testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("\x1b[1;97;41mERROR\x1b[0m"), std::string::npos);
    EXPECT_NE(output.find("network"), std::string::npos);
    EXPECT_NE(output.find("\x1b[1;31mcannot start\x1b[0m"), std::string::npos);
}

TEST_F(LoggerTest, SameTagAlwaysGetsTheSameColor) {
    EXPECT_EQ(rtype::log::style::tag("server"), rtype::log::style::tag("server"));
    EXPECT_NE(rtype::log::style::tag("server"), rtype::log::style::tag("network"));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
