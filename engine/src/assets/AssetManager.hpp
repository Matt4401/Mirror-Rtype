/**
 * @file AssetManager.hpp
 * @brief Engine-wide asset cache: assets named by alias or path, loaded once, held by groups, released with them.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetPath.hpp"
#include "assets/detail/AssetCache.hpp"
#include "assets/detail/AssetContext.hpp"
#include "assets/detail/IAssetCache.hpp"

namespace assets {

/**
 * @brief Knows every asset of the game, and when each one is needed.
 *
 * Games name their assets by alias (`"player_ship"`) instead of by file, and gather them into groups: `"global"`
 * for what the whole game uses, one group per scene or level for the rest. A group is loaded with `loadGroup` (or
 * step by step with `loadGroupStep`, behind a loading screen) and released with `unloadGroup`; an asset shared by two
 * groups stays loaded while either holds it. However many times a file is declared or requested, it is loaded once
 * per set of load options.
 *
 * **Kinds.** The set of kinds is open: `Texture`, `Font`, `Sound` and `Music` are registered by the constructor, and
 * any other type satisfying `AssetKind` is registered on first use (or with `registerKind`), so a game adds its own
 * kinds without touching the engine. Scripts reach a kind by its `NAME`.
 *
 * **Loaders.** The manager loads nothing by itself: backends plug in an `AssetLoader` per kind (the
 * `engine_assets_render` and `engine_assets_audio` bridges do it for the engine's kinds) and detach it when they stop,
 * so this module needs no graphics or audio library and the headless server can link it. Requests made before a
 * loader exists are served as soon as it is installed.
 *
 * **Stable handles.** An asset loaded again (its group was unloaded then loaded, or its backend restarted) asks the
 * backend for the handle it had, so a handle kept by a component or by C++ code names the asset again once it is
 * back. While the asset is unloaded the handle resolves to nothing; it is never given to another asset.
 *
 * **Failures.** Paths, aliases and group names may come from scripts: invalid ones are rejected with a warning
 * (`warnings()`), never with an exception. A file the backend cannot load is `AssetStatus::Failed`: `get` serves the
 * backend's fallback handle (the missing texture checkerboard) and `loadGroup` returns false.
 *
 * **Threads.** Not thread-safe: use it from the thread that created it (the main thread); debug builds assert it.
 *
 * Stored as a registry resource by `AssetPlugin`: `registry.getResource<assets::AssetManager>()`.
 */
class AssetManager {
  public:
    /** @brief Group used when none is given: assets kept for the whole game. */
    static constexpr std::string_view GLOBAL_GROUP = "global";

    /** @brief Most files `preloadDirectory` takes from one directory. */
    static constexpr std::size_t MAX_DIRECTORY_FILES = assets::MAX_DIRECTORY_FILES;

    /** @brief Budget of `loadGroupStep` meaning "everything". */
    static constexpr std::size_t UNLIMITED = std::numeric_limits<std::size_t>::max();

    /**
     * @brief Creates a manager knowing the engine's four kinds, with no loader.
     * @param root Directory relative paths are loaded from, usually `engine::contentRoot()`; empty for the working
     * directory.
     */
    explicit AssetManager(std::filesystem::path root = {});

    /**
     * @brief Destroys the manager without calling the loaders: the backends detach them (and release their assets)
     * when they shut down, and may already be gone here.
     */
    ~AssetManager() = default;

    /** @brief Deleted copy constructor. */
    AssetManager(const AssetManager&) = delete;
    /** @brief Deleted copy assignment operator. */
    AssetManager& operator=(const AssetManager&) = delete;
    /** @brief Deleted move constructor: the caches point to the manager's context. */
    AssetManager(AssetManager&&) = delete;
    /** @brief Deleted move assignment operator. */
    AssetManager& operator=(AssetManager&&) = delete;

    /**
     * @brief Gets the directory relative paths are loaded from.
     * @return Content root; empty for the working directory.
     */
    [[nodiscard]] const std::filesystem::path& root() const noexcept { return _context.root; }

    /**
     * @brief Registers a kind, so that scripts can name it before anything of it is declared from C++.
     *
     * Registering a kind again does nothing.
     * @tparam Kind Kind of asset.
     * @throws std::logic_error if another kind already has the same `NAME`.
     */
    template <AssetKind Kind>
    void registerKind();

    /**
     * @brief Tells whether a kind of this name is registered.
     * @param kind Kind name (`"texture"`).
     * @return True if registered.
     */
    [[nodiscard]] bool hasKind(std::string_view kind) const;

    /**
     * @brief Lists the names of the registered kinds, in registration order.
     * @return Kind names.
     */
    [[nodiscard]] std::vector<std::string_view> kinds() const;

    /**
     * @brief Installs the backend loader of a kind, replacing the previous one; what groups hold is loaded now.
     * @tparam Kind Kind of asset.
     * @param loader Backend loader.
     */
    template <AssetKind Kind>
    void setLoader(AssetLoader<Kind> loader);

    /**
     * @brief Releases every asset of a kind through its loader, then removes the loader.
     *
     * Called by a backend when it shuts down. Groups keep their assets: they are loaded again by the next loader,
     * with the same handles when it can give them.
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
    [[nodiscard]] bool hasLoader() const;

    /**
     * @brief Declares an asset of `group` without loading it; `loadGroup(group)` loads it.
     *
     * Declaring the same thing again changes nothing, so a script declaring its assets can be run again (hot reload).
     * An alias declared again for another file now names that file in all its groups, and the old file is released
     * if nothing else needs it (warning). If the group is already loaded, the asset is loaded at once.
     *
     * @tparam Kind Kind of asset.
     * @param alias Name to find the asset by (no `/`, `\` or `.`); empty to only use the path.
     * @param path File path, relative to the root (or absolute). Aliases and keys keep it as given, so the same
     * declaration names the same asset wherever the game is installed.
     * @param group Group needing it.
     * @param options Load options; the same file with other options is another asset.
     * @return False for an invalid path, alias or group (warning).
     */
    template <AssetKind Kind>
    bool declare(std::string_view alias, std::string_view path, std::string_view group = GLOBAL_GROUP,
                 const LoadOptions& options = {});

    /**
     * @brief Declares an asset of a kind named at run time (scripts).
     * @param kind Kind name.
     * @param alias Alias, or empty.
     * @param path File path.
     * @param group Group.
     * @param options Load options.
     * @return False for an unknown kind or invalid input (warning).
     */
    bool declare(std::string_view kind, std::string_view alias, std::string_view path, std::string_view group,
                 const LoadOptions& options = {});

    /**
     * @brief Removes every declaration of an alias; what no group needs any more is released.
     * @tparam Kind Kind of asset.
     * @param alias Alias.
     * @return False if the alias is unknown.
     */
    template <AssetKind Kind>
    bool undeclare(std::string_view alias);

    /**
     * @brief Removes every declaration of an alias of a kind named at run time.
     * @param kind Kind name.
     * @param alias Alias.
     * @return False for an unknown kind or alias.
     */
    bool undeclare(std::string_view kind, std::string_view alias);

    /**
     * @brief Loads an asset now and makes `group` hold it (it is declared in `group`).
     *
     * @tparam Kind Kind of asset.
     * @param aliasOrPath Declared alias, or a file path (anything with a `/`, `\` or `.`).
     * @param group Group holding it.
     * @param options Load options of a path.
     * @return Its handle, the fallback of a failed asset, or `Kind::NONE` for an unknown alias (warned once) or while
     * no loader is installed.
     */
    template <AssetKind Kind>
    Kind::Id load(std::string_view aliasOrPath, std::string_view group = GLOBAL_GROUP, const LoadOptions& options = {});

    /**
     * @brief Looks up the handle of an asset; loads nothing.
     * @tparam Kind Kind of asset.
     * @param aliasOrPath Alias or file path.
     * @return Its handle, the fallback handle if it failed, or `Kind::NONE` if it is unknown or not loaded.
     */
    template <AssetKind Kind>
    [[nodiscard]] Kind::Id get(std::string_view aliasOrPath) const;

    /**
     * @brief Looks up the handle of an asset of a kind named at run time.
     * @param kind Kind name.
     * @param aliasOrPath Alias or file path.
     * @return Its handle widened to 64 bits, or 0.
     */
    [[nodiscard]] std::uint64_t handle(std::string_view kind, std::string_view aliasOrPath) const;

    /**
     * @brief Tells where an asset stands.
     * @tparam Kind Kind of asset.
     * @param aliasOrPath Alias or file path.
     * @return Its status.
     */
    template <AssetKind Kind>
    [[nodiscard]] AssetStatus status(std::string_view aliasOrPath) const;

    /**
     * @brief Tells where an asset of a kind named at run time stands.
     * @param kind Kind name.
     * @param aliasOrPath Alias or file path.
     * @return Its status; `Unknown` for an unknown kind.
     */
    [[nodiscard]] AssetStatus status(std::string_view kind, std::string_view aliasOrPath) const;

    /**
     * @brief Gets the handle the backend serves for a missing asset of a kind (the texture checkerboard).
     * @tparam Kind Kind of asset.
     * @return The fallback, or `Kind::NONE` without loader or fallback.
     */
    template <AssetKind Kind>
    Kind::Id fallback();

    /**
     * @brief Declares every file of a directory (not recursive) in `group`; they load with the group.
     *
     * Only files with an extension of `Kind::EXTENSIONS` are taken, in name order, at most `MAX_DIRECTORY_FILES`.
     * Each is aliased by its name without extension (`textures/car_body.png` is `"car_body"`); a name that is not a
     * valid alias (it holds a `.`) is skipped with a warning.
     *
     * @tparam Kind Kind of asset.
     * @param directory Directory to scan, relative to the root (or absolute).
     * @param group Group needing the files.
     * @param options Load options.
     * @return Number of files declared; 0 for a missing directory (warning).
     */
    template <AssetKind Kind>
    std::size_t preloadDirectory(const std::filesystem::path& directory, std::string_view group = GLOBAL_GROUP,
                                 const LoadOptions& options = {});

    /**
     * @brief Declares the files of a directory for a kind named at run time.
     * @param kind Kind name.
     * @param directory Directory to scan.
     * @param group Group.
     * @param options Load options.
     * @return Number of files declared; 0 for an unknown kind or a missing directory (warning).
     */
    std::size_t preloadDirectory(std::string_view kind, const std::filesystem::path& directory, std::string_view group,
                                 const LoadOptions& options = {});

    /**
     * @brief Loads every asset declared in a group and keeps the group loaded: assets declared in it later are
     * loaded at once. Loading a loaded group again only loads what is still missing.
     *
     * @param group Group to load.
     * @return True if every asset of the group is loaded (false if one failed, or while a kind has no loader yet).
     */
    bool loadGroup(std::string_view group);

    /**
     * @brief Loads at most `maxLoads` assets of a group, so that a loading screen can draw between two steps.
     *
     * The first step marks the group loaded and makes it hold all its assets; call it every frame until
     * `done()`.
     * @param group Group to load.
     * @param maxLoads Most files to load in this step.
     * @return Progress after the step.
     */
    GroupProgress loadGroupStep(std::string_view group, std::size_t maxLoads);

    /**
     * @brief Tells how far the loading of a group went.
     * @param group Group.
     * @return Its progress; an empty progress for an unknown group.
     */
    [[nodiscard]] GroupProgress groupProgress(std::string_view group) const;

    /**
     * @brief Releases the assets of a group that no other group holds; the declarations are kept.
     * @param group Group to release.
     */
    void unloadGroup(std::string_view group);

    /**
     * @brief Unloads a group and removes its declarations.
     * @param group Group to forget.
     */
    void forgetGroup(std::string_view group);

    /**
     * @brief Tells whether a group is loaded.
     * @param group Group name.
     * @return True between `loadGroup` (or the first `loadGroupStep`) and `unloadGroup`.
     */
    [[nodiscard]] bool isGroupLoaded(std::string_view group) const;

    /** @brief Releases every asset of every group; the declarations are kept. */
    void unloadAll();

    /**
     * @brief Counts the distinct assets of a kind currently loaded (fallbacks excluded).
     * @tparam Kind Kind of asset.
     * @return Number of loaded assets.
     */
    template <AssetKind Kind>
    [[nodiscard]] std::size_t loadedCount() const;

    /**
     * @brief Opens the window in which a declaration script runs.
     *
     * Declarations made until `endScriptDeclarations` belong to the script. With `replacing`, the ones it made
     * before are marked stale first: those it does not make again are removed at the end, so deleting a line of the
     * script and hot reloading it releases the asset. Declarations made from C++ or outside the window are kept.
     * @param replacing True for a hot reload of the scripts.
     */
    void beginScriptDeclarations(bool replacing);

    /**
     * @brief Closes the window opened by `beginScriptDeclarations`.
     * @param dropStale True to drop the stale declarations; false when the scripts failed to run again, so that
     * the declarations they did not reach stay (and their assets stay loaded).
     */
    void endScriptDeclarations(bool dropStale = true);

    /**
     * @brief Reports a problem found by a caller of the manager (a bridge rejecting script input).
     * @param message Warning, printed with the `[assets]` prefix and kept.
     */
    void warn(std::string message) { _context.diagnostics.warn(std::move(message)); }

    /**
     * @brief Reports a problem once per `key`.
     * @param key What the warning is about.
     * @param message Warning.
     */
    void warnOnce(std::string_view key, std::string message) { _context.diagnostics.warnOnce(key, std::move(message)); }

    /**
     * @brief Gets the latest warnings (invalid names, missing files...), oldest first.
     * @return Warnings, also printed on the error output.
     */
    [[nodiscard]] const std::deque<std::string>& warnings() const noexcept { return _context.diagnostics.warnings(); }

    /** @brief Forgets the warnings. */
    void clearWarnings() noexcept { _context.diagnostics.clear(); }

  private:
    /**
     * @brief Gets the cache of a kind, registering the kind if needed.
     * @tparam Kind Kind of asset.
     * @return Its cache.
     */
    template <AssetKind Kind>
    [[nodiscard]] detail::AssetCache<Kind>& cache();

    /**
     * @brief Gets the cache of a kind, if registered.
     * @tparam Kind Kind of asset.
     * @return Its cache, or null.
     */
    template <AssetKind Kind>
    [[nodiscard]] const detail::AssetCache<Kind>* findCache() const;

    /**
     * @brief Gets the cache of a kind named at run time.
     * @param kind Kind name.
     * @return Its cache, or null.
     */
    [[nodiscard]] detail::IAssetCache* findCache(std::string_view kind) const;

    /**
     * @brief Gets the cache of a kind named at run time, warning when there is none.
     * @param kind Kind name.
     * @return Its cache, or null.
     */
    [[nodiscard]] detail::IAssetCache* cacheOrWarn(std::string_view kind);

    /** @brief Asserts, in debug builds, that the manager is used from the thread that created it. */
    void checkThread() const noexcept;

    /** @brief Warnings, loaded groups, script window and root, shared with the caches; declared first, built first. */
    detail::AssetContext _context;
    /** @brief One cache per registered kind, in registration order. */
    std::vector<std::unique_ptr<detail::IAssetCache>> _caches;
    /** @brief Index in `_caches` of each kind type. */
    std::unordered_map<std::type_index, std::size_t> _byType;
    /** @brief Thread that created the manager. */
    std::thread::id _owner;
};

}  // namespace assets

#include "assets/AssetManager.tpp"  // IWYU pragma: export
