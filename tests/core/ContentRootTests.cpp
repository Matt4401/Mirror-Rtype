#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#include "core/ContentRoot.hpp"

namespace {

/** @brief Sets an environment variable for the scope of a test, then unsets it. */
class ScopedEnvironment {
  public:
    ScopedEnvironment(const std::string& name, const std::string& value) : _name(name) { set(value.c_str()); }
    ~ScopedEnvironment() { set(nullptr); }
    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;
    ScopedEnvironment(ScopedEnvironment&&) = delete;
    ScopedEnvironment& operator=(ScopedEnvironment&&) = delete;

  private:
    void set(const char* value) const {
#ifdef _WIN32
        _putenv_s(_name.c_str(), value != nullptr ? value : "");
#else
        if (value != nullptr) {
            setenv(_name.c_str(), value, 1);
        } else {
            unsetenv(_name.c_str());
        }
#endif
    }

    std::string _name;
};

}  // namespace

TEST(ContentRoot, ExecutableDirectoryHoldsTheTestBinary) {
    const std::filesystem::path directory = engine::executableDirectory();
    ASSERT_FALSE(directory.empty());
    EXPECT_TRUE(directory.is_absolute());
    std::error_code error;
    EXPECT_TRUE(std::filesystem::is_directory(directory, error));
}

TEST(ContentRoot, DefaultsToTheExecutableDirectory) { EXPECT_EQ(engine::contentRoot(), engine::executableDirectory()); }

TEST(ContentRoot, TheEnvironmentVariableOverridesItWhenItNamesADirectory) {
    const std::filesystem::path temp = std::filesystem::temp_directory_path();
    {
        const ScopedEnvironment variable(std::string(engine::CONTENT_ROOT_VARIABLE), temp.string());
        EXPECT_EQ(engine::contentRoot(), temp);
    }
    {
        const ScopedEnvironment variable(std::string(engine::CONTENT_ROOT_VARIABLE),
                                         (temp / "no_such_directory_for_content_root").string());
        EXPECT_EQ(engine::contentRoot(), engine::executableDirectory());
    }
}

TEST(ContentRoot, ResolvesRelativePathsAgainstTheRoot) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "game";
    EXPECT_EQ(engine::resolveContentPath(root, "sprites/ship.png"), root / "sprites/ship.png");
    EXPECT_EQ(engine::resolveContentPath({}, "sprites/ship.png"), std::filesystem::path("sprites/ship.png"));
    const std::filesystem::path absolute = std::filesystem::temp_directory_path() / "ship.png";
    EXPECT_EQ(engine::resolveContentPath(root, absolute), absolute);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
