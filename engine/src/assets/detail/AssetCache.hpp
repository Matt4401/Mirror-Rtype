/**
 * @file AssetCache.hpp
 * @brief Cache of one kind of asset: entries by path and alias, the groups holding them, the backend loader.
 */

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/detail/Diagnostics.hpp"

namespace assets::detail {

/** @brief Transparent string hash, so that maps keyed by `std::string` are searched with a `std::string_view`. */
struct StringHash {
    /** @brief Enables heterogeneous lookup. */
    using is_transparent = void;

    /**
     * @brief Hashes a string.
     * @param text String to hash.
     * @return Hash.
     */
    [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
        return std::hash<std::string_view>{}(text);
    }
};

/**
 * @brief Every asset of one kind known to the manager.
 *
 * An entry is created per normalised path, so a file is loaded at most once whatever the number of aliases, groups
 * or requests naming it. It is loaded while at least one group holds it, and released when the last one lets it go.
 * Entries are never removed: declarations stay known after an unload, so loading the group again finds them.
 *
 * @tparam Kind Kind of asset.
 */
template <AssetKind Kind>
class AssetCache {
  public:
    /** @brief Handle type of the kind. */
    using Id = Kind::Id;

    /**
     * @brief Creates an empty cache reporting to `diagnostics`.
     * @param diagnostics Warning sink, owned by the manager and outliving the cache.
     */
    explicit AssetCache(Diagnostics& diagnostics) noexcept : _diagnostics(&diagnostics) {}

    /**
     * @brief Records that `group` uses the file at `path`, under `alias` if not empty. Loads nothing.
     *
     * @param alias Name to find the asset by, or empty.
     * @param path File path.
     * @param group Group the declaration belongs to.
     * @param options Load options, kept from the first declaration of the file.
     * @return The entry, or nothing for an invalid path or name (a warning is emitted).
     */
    std::optional<std::size_t> declare(std::string_view alias, std::string_view path, std::string_view group,
                                       const LoadOptions& options);

    /**
     * @brief Finds the entry of an alias or a path, without creating it.
     * @param aliasOrPath Alias, or file path.
     * @return The entry, or nothing.
     */
    [[nodiscard]] std::optional<std::size_t> find(std::string_view aliasOrPath) const;

    /**
     * @brief Finds the entry of an alias or a path, creating one for an unknown path.
     * @param aliasOrPath Alias, or file path.
     * @param options Load options of a new entry.
     * @return The entry, or nothing for an unknown alias that is not a valid path either.
     */
    std::optional<std::size_t> findOrCreate(std::string_view aliasOrPath, const LoadOptions& options);

    /**
     * @brief Makes `group` hold an entry, loading it if it is not loaded yet and a loader is installed.
     * @param entry Entry index.
     * @param group Group taking the asset.
     * @return Its handle, or `Kind::NONE` while it is not loaded.
     */
    Id acquire(std::size_t entry, std::string_view group);

    /**
     * @brief Makes `group` hold every entry it declared.
     * @param group Group to load.
     * @return Number of these entries that are not loaded after the call.
     */
    std::size_t acquireDeclared(std::string_view group);

    /**
     * @brief Makes `group` let every entry go, releasing the ones no other group holds.
     * @param group Group to release.
     */
    void release(std::string_view group);

    /** @brief Releases every loaded entry; no group holds anything afterwards. */
    void releaseAll();

    /**
     * @brief Installs the backend loader, replacing (and detaching) the previous one.
     *
     * Entries already held by a group, requested before any loader existed, are loaded now.
     * @param loader Backend loader.
     */
    void setLoader(AssetLoader<Kind> loader);

    /**
     * @brief Releases every loaded entry through the loader, then removes it.
     *
     * Groups keep holding their entries, which are loaded again if a loader is installed later.
     */
    void detachLoader();

    /**
     * @brief Tells whether a loader is installed.
     * @return True once `setLoader` was called and until `detachLoader`.
     */
    [[nodiscard]] bool hasLoader() const noexcept { return _loader.has_value(); }

    /**
     * @brief Gets the handle of an entry.
     * @param entry Entry index.
     * @return Its handle, or `Kind::NONE` while it is not loaded.
     */
    [[nodiscard]] Id id(std::size_t entry) const noexcept { return _entries.at(entry).id; }

    /**
     * @brief Counts the loaded entries.
     * @return Number of distinct files loaded.
     */
    [[nodiscard]] std::size_t loadedCount() const noexcept;

  private:
    /** @brief One file. */
    struct Entry {
        /** @brief Normalised path. */
        std::string path;
        /** @brief Options of the first declaration or request. */
        LoadOptions options;
        /** @brief Handle while loaded, `Kind::NONE` otherwise. */
        Id id = Kind::NONE;
        /** @brief The loader failed: not tried again until the entry is released or a new loader is installed. */
        bool failed = false;
        /** @brief Groups that declared the entry. */
        std::vector<std::string> declaredIn;
        /** @brief Groups holding the entry; it is loaded while this is not empty. */
        std::vector<std::string> holders;
    };

    /**
     * @brief Loads an entry if it is held, not loaded, not failed, and a loader exists.
     * @param entry Entry to load.
     */
    void ensureLoaded(Entry& entry);

    /**
     * @brief Releases a loaded entry through the loader.
     * @param entry Entry to release.
     */
    void unloadEntry(Entry& entry);

    /**
     * @brief Creates the entry of a normalised path, or returns the existing one.
     * @param key Normalised path.
     * @param options Options of a new entry.
     * @return Entry index.
     */
    std::size_t entryFor(const std::string& key, const LoadOptions& options);

    /** @brief Warning sink of the manager. */
    Diagnostics* _diagnostics;
    /** @brief Entries; indices are stable. */
    std::vector<Entry> _entries;
    /** @brief Entry of each normalised path. */
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> _byPath;
    /** @brief Entry of each alias. */
    std::unordered_map<std::string, std::size_t, StringHash, std::equal_to<>> _byAlias;
    /** @brief Backend loader, if one is installed. */
    std::optional<AssetLoader<Kind>> _loader;
};

}  // namespace assets::detail

#include "assets/detail/AssetCache.tpp"  // IWYU pragma: export
