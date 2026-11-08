/**
 * @file AssetManager.hpp
 * @brief Engine-wide asset cache: assets named by alias or path, loaded once, held by groups, released with them.
 */

#pragma once

#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/detail/AssetCache.hpp"
#include "assets/detail/Diagnostics.hpp"

namespace assets {

/**
 * @brief Knows every texture, font, sound and music of the game, and when each one is needed.
 *
 * Games name their assets by alias (`"player_ship"`) instead of by file, and gather them into groups: `"global"`
 * for what the whole game uses, one group per scene or level for the rest. A group is loaded with `loadGroup` (while
 * a loading screen is shown, or on entering a scene) and released with `unloadGroup`; an asset shared by two groups
 * stays loaded while either holds it. However many times a file is declared or requested, it is loaded once.
 *
 * The manager loads nothing by itself: the rendering and audio modules plug in their `AssetLoader`s when they start
 * and detach them when they stop, so this module needs no graphics or audio library and the headless server can link
 * it. Requests made before a loader exists are served as soon as it is installed.
 *
 * Paths, aliases and group names may come from scripts: invalid ones are rejected with a warning (`warnings()`),
 * never with an exception.
 *
 * Stored as a registry resource by `AssetPlugin`: `registry.getResource<assets::AssetManager>()`.
 */
class AssetManager {
  public:
    /** @brief Group used when none is given: assets kept for the whole game. */
    static constexpr std::string_view GLOBAL_GROUP = "global";

    /** @brief Most files `preloadDirectory` takes from one directory. */
    static constexpr std::size_t MAX_DIRECTORY_FILES = 1024;

    /** @brief Creates an empty manager with no loader. */
    AssetManager();

    /**
     * @brief Destroys the manager without calling the loaders: the backends detach them (and release their assets)
     * when they shut down, and may already be gone here.
     */
    ~AssetManager() = default;

    /** @brief Deleted copy constructor. */
    AssetManager(const AssetManager&) = delete;
    /** @brief Deleted copy assignment operator. */
    AssetManager& operator=(const AssetManager&) = delete;
    /** @brief Deleted move constructor: the caches point to the manager's diagnostics. */
    AssetManager(AssetManager&&) = delete;
    /** @brief Deleted move assignment operator. */
    AssetManager& operator=(AssetManager&&) = delete;

    /**
     * @brief Installs the backend loader of a kind, replacing the previous one.
     *
     * Assets already requested by a group are loaded now.
     * @tparam Kind Kind of asset.
     * @param loader Backend loader.
     */
    template <AssetKind Kind>
    void setLoader(AssetLoader<Kind> loader);

    /**
     * @brief Releases every asset of a kind through its loader, then removes the loader.
     *
     * Called by a backend when it shuts down. Groups keep their assets: they are loaded again by the next loader.
     * @tparam Kind Kind of asset.
     */
    template <AssetKind Kind>
    void detachLoader();

    /**
     * @brief Tells whether a kind has a loader.
     * @tparam Kind Kind of asset.
     * @return True while a backend is plugged in.
     */
    template <AssetKind Kind>
    [[nodiscard]] bool hasLoader() const noexcept;

    /**
     * @brief Declares an asset of `group` without loading it; `loadGroup(group)` loads it.
     *
     * Declaring the same thing again changes nothing, so a script declaring its assets can be run again (hot reload).
     * An alias declared again for another file now names that file (warning). If the group is already loaded, the
     * asset is loaded at once.
     *
     * @tparam Kind Kind of asset.
     * @param alias Name to find the asset by; empty to only use the path.
     * @param path File path, relative to the working directory.
     * @param group Group needing it.
     * @param options Load options; the ones of the first declaration of a file are kept.
     * @return False for an invalid path, alias or group (warning).
     */
    template <AssetKind Kind>
    bool declare(std::string_view alias, std::string_view path, std::string_view group = GLOBAL_GROUP,
                 const LoadOptions& options = {});

    /**
     * @brief Loads an asset now and makes `group` hold it.
     *
     * @tparam Kind Kind of asset.
     * @param aliasOrPath Declared alias, or a file path (declared on the fly, without alias).
     * @param group Group holding it.
     * @param options Load options of a file not known yet.
     * @return Its handle, or `Kind::NONE` if it cannot be loaded or no loader is installed yet.
     */
    template <AssetKind Kind>
    Kind::Id load(std::string_view aliasOrPath, std::string_view group = GLOBAL_GROUP, const LoadOptions& options = {});

    /**
     * @brief Looks up the handle of a loaded asset; loads nothing.
     * @tparam Kind Kind of asset.
     * @param aliasOrPath Alias or file path.
     * @return Its handle, or `Kind::NONE` if it is unknown or not loaded.
     */
    template <AssetKind Kind>
    [[nodiscard]] Kind::Id get(std::string_view aliasOrPath) const;

    /**
     * @brief Declares every file of a directory (not recursive) in `group` and loads them at once.
     *
     * Only files with an extension of `Kind::EXTENSIONS` are taken, in name order, at most `MAX_DIRECTORY_FILES`.
     * Each is aliased by its name without extension (`textures/car_body.png` is `"car_body"`).
     *
     * @tparam Kind Kind of asset.
     * @param directory Directory to scan.
     * @param group Group holding the files.
     * @param options Load options.
     * @return Number of files taken; 0 for a missing directory (warning).
     */
    template <AssetKind Kind>
    std::size_t preloadDirectory(const std::filesystem::path& directory, std::string_view group = GLOBAL_GROUP,
                                 const LoadOptions& options = {});

    /**
     * @brief Loads every asset declared in a group and keeps the group loaded: assets declared in it later are
     * loaded at once. Loading a loaded group again does nothing more.
     *
     * @param group Group to load.
     * @return True if every asset of the group is loaded (false also while a kind has no loader yet).
     */
    bool loadGroup(std::string_view group);

    /**
     * @brief Releases the assets of a group that no other group holds; the declarations are kept.
     * @param group Group to release.
     */
    void unloadGroup(std::string_view group);

    /**
     * @brief Tells whether a group is loaded.
     * @param group Group name.
     * @return True between `loadGroup` and `unloadGroup`.
     */
    [[nodiscard]] bool isGroupLoaded(std::string_view group) const;

    /** @brief Releases every asset of every group; the declarations are kept. */
    void unloadAll();

    /**
     * @brief Counts the distinct files of a kind currently loaded.
     * @tparam Kind Kind of asset.
     * @return Number of loaded files.
     */
    template <AssetKind Kind>
    [[nodiscard]] std::size_t loadedCount() const noexcept;

    /**
     * @brief Gets the latest warnings (invalid names, missing files...), oldest first.
     * @return Warnings, also printed on the error output.
     */
    [[nodiscard]] std::span<const std::string> warnings() const noexcept { return _diagnostics.warnings(); }

    /** @brief Forgets the warnings. */
    void clearWarnings() noexcept { _diagnostics.clear(); }

  private:
    /**
     * @brief Gets the cache of a kind.
     * @tparam Kind Kind of asset.
     * @return Its cache.
     */
    template <AssetKind Kind>
    [[nodiscard]] detail::AssetCache<Kind>& cache() noexcept {
        return std::get<detail::AssetCache<Kind>>(_caches);
    }

    /**
     * @brief Gets the cache of a kind.
     * @tparam Kind Kind of asset.
     * @return Its cache.
     */
    template <AssetKind Kind>
    [[nodiscard]] const detail::AssetCache<Kind>& cache() const noexcept {
        return std::get<detail::AssetCache<Kind>>(_caches);
    }

    /** @brief Warning sink of the caches; declared first, so it is built before them. */
    detail::Diagnostics _diagnostics;
    /** @brief One cache per kind. */
    std::tuple<detail::AssetCache<Texture>, detail::AssetCache<Font>, detail::AssetCache<Sound>,
               detail::AssetCache<Music>>
        _caches;
    /** @brief Groups loaded with `loadGroup` and not unloaded since. */
    std::vector<std::string> _loadedGroups;
};

}  // namespace assets

#include "assets/AssetManager.tpp"  // IWYU pragma: export
