#include "core/ContentRoot.hpp"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)  // NOLINT(readability-use-concise-preprocessor-directives): #elifdef is C++23 only
#include <mach-o/dyld.h>

#include <cstdint>
#include <vector>
#endif

namespace engine {

namespace {

/**
 * @brief Gets the absolute path of the running executable.
 * @return Its path, or an empty path if the platform cannot tell.
 */
std::filesystem::path executablePath() {
#ifdef _WIN32
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size()) {
            buffer.resize(length);
            return std::filesystem::path(buffer);
        }
        buffer.resize(buffer.size() * 2);
    }
#elif defined(__APPLE__)  // NOLINT(readability-use-concise-preprocessor-directives): #elifdef is C++23 only
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    std::error_code error;
    const std::filesystem::path path = std::filesystem::weakly_canonical(buffer.data(), error);
    return error ? std::filesystem::path(buffer.data()) : path;
#else
    std::error_code error;
    const std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
    return error ? std::filesystem::path{} : path;
#endif
}

}  // namespace

std::filesystem::path executableDirectory() {
    const std::filesystem::path path = executablePath();
    return path.empty() ? path : path.parent_path();
}

std::filesystem::path contentRoot() {
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at start-up, never written by the engine.
    const char* overridden = std::getenv(std::string(CONTENT_ROOT_VARIABLE).c_str());
    if (overridden != nullptr && *overridden != '\0') {
        std::error_code error;
        if (std::filesystem::is_directory(overridden, error)) {
            return {overridden};
        }
    }
    return executableDirectory();
}

std::filesystem::path resolveContentPath(const std::filesystem::path& root, const std::filesystem::path& path) {
    if (root.empty() || path.is_absolute()) {
        return path;
    }
    return root / path;
}

}  // namespace engine
