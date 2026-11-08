#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "core/ContentRoot.hpp"

namespace assets {

namespace manager_detail {

/**
 * @brief Tells whether a file has one of the extensions of a kind, whatever their case.
 * @tparam Kind Kind of asset.
 * @param file File path.
 * @return True for a matching extension.
 */
template <AssetKind Kind>
[[nodiscard]] bool hasExtensionOf(const std::filesystem::path& file) {
    std::string extension = file.extension().string();
    std::ranges::transform(extension, extension.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return std::ranges::find(Kind::EXTENSIONS, std::string_view(extension)) != Kind::EXTENSIONS.end();
}

}  // namespace manager_detail

template <AssetKind Kind>
void AssetManager::setLoader(AssetLoader<Kind> loader) {
    cache<Kind>().setLoader(std::move(loader));
}

template <AssetKind Kind>
void AssetManager::detachLoader() {
    cache<Kind>().detachLoader();
}

template <AssetKind Kind>
bool AssetManager::hasLoader() const noexcept {
    return cache<Kind>().hasLoader();
}

template <AssetKind Kind>
bool AssetManager::declare(const std::string_view alias, const std::string_view path, const std::string_view group,
                           const LoadOptions& options) {
    const std::optional<std::size_t> entry = cache<Kind>().declare(alias, path, group, options);
    if (!entry) {
        return false;
    }
    if (isGroupLoaded(group)) {
        cache<Kind>().acquire(*entry, group);
    }
    return true;
}

template <AssetKind Kind>
Kind::Id AssetManager::load(const std::string_view aliasOrPath, const std::string_view group,
                            const LoadOptions& options) {
    if (!isValidAssetName(group)) {
        _diagnostics.warn(std::string(Kind::NAME) + " '" + std::string(aliasOrPath.substr(0, MAX_NAME_LENGTH)) +
                          "': invalid group name");
        return Kind::NONE;
    }
    const std::optional<std::size_t> entry = cache<Kind>().findOrCreate(aliasOrPath, options);
    if (!entry) {
        return Kind::NONE;
    }
    return cache<Kind>().acquire(*entry, group);
}

template <AssetKind Kind>
Kind::Id AssetManager::get(const std::string_view aliasOrPath) const {
    const std::optional<std::size_t> entry = cache<Kind>().find(aliasOrPath);
    return entry ? cache<Kind>().id(*entry) : Kind::NONE;
}

template <AssetKind Kind>
std::size_t AssetManager::preloadDirectory(const std::filesystem::path& directory, const std::string_view group,
                                           const LoadOptions& options) {
    const std::filesystem::path scanned = engine::resolveContentPath(_root, directory);
    std::error_code error;
    if (!std::filesystem::is_directory(scanned, error)) {
        _diagnostics.warn(std::string(Kind::NAME) + " directory " + directory.generic_string() + " not found");
        return 0;
    }
    std::vector<std::filesystem::path> files;
    for (auto it = std::filesystem::directory_iterator(scanned, error);
         !error && it != std::filesystem::directory_iterator(); it.increment(error)) {
        std::error_code typeError;
        if (it->is_regular_file(typeError) && manager_detail::hasExtensionOf<Kind>(it->path())) {
            // Declared under the path as given, relative to the root like every other key.
            files.push_back(directory / it->path().filename());
        }
    }
    std::ranges::sort(files);
    if (files.size() > MAX_DIRECTORY_FILES) {
        _diagnostics.warn(directory.generic_string() + ": only the first " + std::to_string(MAX_DIRECTORY_FILES) +
                          " files are taken");
        files.resize(MAX_DIRECTORY_FILES);
    }
    std::size_t taken = 0;
    for (const std::filesystem::path& file : files) {
        const std::string alias = file.stem().string();
        const std::optional<std::size_t> entry = cache<Kind>().declare(alias, file.generic_string(), group, options);
        if (entry) {
            cache<Kind>().acquire(*entry, group);
            ++taken;
        }
    }
    return taken;
}

template <AssetKind Kind>
std::size_t AssetManager::loadedCount() const noexcept {
    return cache<Kind>().loadedCount();
}

}  // namespace assets
