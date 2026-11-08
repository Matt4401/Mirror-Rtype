#include "tools/console/ScriptFile.hpp"

#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace tools {

std::expected<std::filesystem::path, std::string> resolveScriptPath(const std::filesystem::path& root,
                                                                    std::string_view relative) {
    const std::filesystem::path typed(relative);
    if (relative.empty() || typed.has_root_path()) {
        return std::unexpected("'" + std::string(relative) + "': give a path relative to the script directory");
    }
    std::error_code error;
    const std::filesystem::path base = std::filesystem::weakly_canonical(root.empty() ? "." : root, error);
    if (error) {
        return std::unexpected("Script directory unavailable: " + error.message());
    }
    // Symbolic links and `..` are resolved before the check, so neither can lead outside the root.
    std::filesystem::path resolved = std::filesystem::weakly_canonical(base / typed, error);
    if (error) {
        return std::unexpected("'" + std::string(relative) + "': " + error.message());
    }
    const std::filesystem::path inside = resolved.lexically_relative(base);
    if (inside.empty() || *inside.begin() == "..") {
        return std::unexpected("'" + std::string(relative) + "' is outside the script directory");
    }
    return resolved;
}

std::expected<std::string, std::string> readScriptFile(const std::filesystem::path& root, std::string_view relative) {
    auto path = resolveScriptPath(root, relative);
    if (!path) {
        return std::unexpected(std::move(path.error()));
    }
    const std::string name = "'" + std::string(relative) + "'";
    std::error_code error;
    if (!std::filesystem::is_regular_file(*path, error)) {
        return std::unexpected(name + ": no such script file");
    }
    const auto size = std::filesystem::file_size(*path, error);
    if (error) {
        return std::unexpected(name + ": " + error.message());
    }
    if (size > MAX_SCRIPT_SIZE) {
        return std::unexpected(name + " is larger than " + std::to_string(MAX_SCRIPT_SIZE) + " bytes");
    }
    std::ifstream file(*path, std::ios::binary);
    if (!file) {
        return std::unexpected(name + ": cannot be opened");
    }
    // Read at most one byte past the limit: a file that grew since it was measured is refused, never loaded whole.
    std::string content(static_cast<std::size_t>(size) + 1, '\0');
    file.read(content.data(), static_cast<std::streamsize>(content.size()));
    content.resize(static_cast<std::size_t>(file.gcount()));
    if (content.size() > MAX_SCRIPT_SIZE) {
        return std::unexpected(name + " is larger than " + std::to_string(MAX_SCRIPT_SIZE) + " bytes");
    }
    return content;
}

}  // namespace tools
