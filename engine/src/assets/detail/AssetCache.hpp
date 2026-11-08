/**
 * @file AssetCache.hpp
 * @brief Cache of one kind of asset: entries by file and options, aliases, declarations per group, the loader.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/detail/AssetContext.hpp"
#include "assets/detail/IAssetCache.hpp"

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

/** @brief Map keyed by strings, searchable with a `std::string_view`. */
template <typename Value>
using StringMap = std::unordered_map<std::string, Value, StringHash, std::equal_to<>>;

/**
 * @brief Every asset of one kind known to the manager.
 *
 * An entry is created per normalised path and load options, so a file is loaded at most once per set of options
 * whatever the number of aliases, groups or requests naming it. It is loaded while at least one group holds it, and
 * released when the last one lets it go. An entry lives while a declaration names it: declarations stay after an
 * unload, so loading the group again finds them, and the handle the entry had is asked for again (stable handles).
 *
 * Every group holding an entry has declared it, so a group is released by walking its own entries only.
 *
 * @tparam Kind Kind of asset.
 */
template <AssetKind Kind>
class AssetCache final : public IAssetCache {
  public:
    /** @brief Handle type of the kind. */
    using Id = Kind::Id;

    /**
     * @brief Creates an empty cache.
     * @param context State shared with the manager, outliving the cache.
     */
    explicit AssetCache(AssetContext& context) noexcept : _context(&context) {}

    /**
     * @brief Records that `group` uses the file at `path`, under `alias` if not empty; loads it if the group is loaded.
     *
     * An alias declared again for another file now names that file in every group it was declared in (warning).
     * @param alias Name to find the asset by, or empty.
     * @param path File path.
     * @param group Group the declaration belongs to.
     * @param options Load options.
     * @return The entry, or nothing for an invalid path or name (a warning is emitted).
     */
    std::optional<std::size_t> declare(std::string_view alias, std::string_view path, std::string_view group,
                                       const LoadOptions& options);

    /**
     * @brief Makes `group` hold the asset named by an alias or a path, declaring it in `group`, and loads it.
     * @param aliasOrPath Declared alias, or a file path.
     * @param group Group holding it.
     * @param options Load options of a path.
     * @return Its handle (the fallback for a failed one), or `Kind::NONE`.
     */
    Id load(std::string_view aliasOrPath, std::string_view group, const LoadOptions& options);

    /**
     * @brief Gets the handle served for an alias or a path; loads nothing.
     * @param aliasOrPath Alias or path.
     * @return Its handle, the fallback handle if it failed, or `Kind::NONE`.
     */
    [[nodiscard]] Id get(std::string_view aliasOrPath) const;

    /**
     * @brief Gets the backend's fallback handle, asking the loader for it the first time.
     * @return The fallback, or `Kind::NONE` if there is no loader or it has none.
     */
    Id fallback();

    /**
     * @brief Installs the backend loader, replacing (and detaching) the previous one, then loads what groups hold.
     * @param loader Backend loader.
     */
    void setLoader(AssetLoader<Kind> loader);

    /** @brief Releases every loaded entry through the loader, then removes it; groups keep holding their entries. */
    void detachLoader();

    /**
     * @brief Tells whether a loader is installed.
     * @return True once `setLoader` was called and until `detachLoader`.
     */
    [[nodiscard]] bool hasLoader() const noexcept { return _loader.has_value(); }

    /**
     * @brief Counts the loaded entries (fallbacks excluded).
     * @return Number of distinct assets loaded.
     */
    [[nodiscard]] std::size_t loadedCount() const noexcept;

    /** @copydoc IAssetCache::kindName */
    [[nodiscard]] std::string_view kindName() const noexcept override { return Kind::NAME; }
    /** @copydoc IAssetCache::declareAny */
    bool declareAny(std::string_view alias, std::string_view path, std::string_view group,
                    const LoadOptions& options) override {
        return declare(alias, path, group, options).has_value();
    }
    /** @copydoc IAssetCache::undeclare */
    bool undeclare(std::string_view alias) override;
    /** @copydoc IAssetCache::preloadAny */
    std::size_t preloadAny(const std::filesystem::path& directory, std::string_view group,
                           const LoadOptions& options) override;
    /** @copydoc IAssetCache::handleOf */
    [[nodiscard]] std::uint64_t handleOf(std::string_view aliasOrPath) const override {
        return static_cast<std::uint64_t>(get(aliasOrPath));
    }
    /** @copydoc IAssetCache::statusOf */
    [[nodiscard]] AssetStatus statusOf(std::string_view aliasOrPath) const override;
    /** @copydoc IAssetCache::holdGroup */
    void holdGroup(std::string_view group) override;
    /** @copydoc IAssetCache::loadPending */
    std::size_t loadPending(std::string_view group, std::size_t budget) override;
    /** @copydoc IAssetCache::addProgress */
    void addProgress(std::string_view group, GroupProgress& progress) const override;
    /** @copydoc IAssetCache::release */
    void release(std::string_view group) override;
    /** @copydoc IAssetCache::forgetGroup */
    void forgetGroup(std::string_view group) override;
    /** @copydoc IAssetCache::releaseAll */
    void releaseAll() override;
    /** @copydoc IAssetCache::markScriptDeclarationsStale */
    void markScriptDeclarationsStale() override;
    /** @copydoc IAssetCache::dropStaleDeclarations */
    void dropStaleDeclarations() override;

  private:
    /** @brief One reason for an entry to exist: a group needs it, maybe under an alias. */
    struct Declaration {
        /** @brief Alias, or empty for a declaration by path. */
        std::string alias;
        /** @brief Group needing the entry. */
        std::string group;
        /** @brief Made while a declaration script ran: a hot reload of the script may drop it. */
        bool fromScript = false;
        /** @brief Not made again (yet) by the hot reload in progress. */
        bool stale = false;
    };

    /** @brief One file loaded with one set of options. */
    struct Entry {
        /** @brief False for a free slot of `_entries`. */
        bool used = false;
        /** @brief Normalised path. */
        std::string path;
        /** @brief Key in `_byKey`: path and options. */
        std::string key;
        /** @brief Options it is loaded with. */
        LoadOptions options;
        /** @brief Handle while loaded, `Kind::NONE` otherwise. */
        Id id = Kind::NONE;
        /** @brief Handle of the last load, asked for again by the next one so that handles stay stable. */
        Id lastId = Kind::NONE;
        /** @brief The loader failed: not tried again until the entry is released or a new loader is installed. */
        bool failed = false;
        /** @brief Why the entry exists. */
        std::vector<Declaration> declarations;
        /** @brief Groups holding the entry; it is loaded while this is not empty. */
        std::vector<std::string> holders;
    };

    /**
     * @brief Finds the entry of an alias or a path, without creating it.
     * @param aliasOrPath Alias, or path (see `looksLikeAssetPath`).
     * @return The entry, or nothing.
     */
    [[nodiscard]] std::optional<std::size_t> find(std::string_view aliasOrPath) const;

    /**
     * @brief Makes `group` hold an entry and loads it if needed.
     * @param index Entry index.
     * @param group Group taking the asset.
     */
    void acquire(std::size_t index, std::string_view group);

    /**
     * @brief Makes `group` let an entry go, releasing it when no group holds it any more.
     * @param index Entry index.
     * @param group Group.
     */
    void dropHold(std::size_t index, std::string_view group);

    /**
     * @brief Adds a declaration to an entry (or refreshes the same one), and the entry to its group's list.
     * @param index Entry index.
     * @param alias Alias, or empty.
     * @param group Group.
     */
    void addDeclaration(std::size_t index, std::string_view alias, std::string_view group);

    /**
     * @brief Removes the declarations of an entry matching `predicate`, releasing the groups that no longer declare
     * it and freeing the entry if nothing declares it any more.
     * @param index Entry index.
     * @param predicate Takes a `const Declaration&`.
     */
    template <typename Predicate>
    void removeDeclarations(std::size_t index, const Predicate& predicate);

    /**
     * @brief Moves the declarations made under `alias` from an entry to another one.
     * @param from Entry the alias named.
     * @param to Entry it now names.
     * @param alias Alias.
     */
    void moveAlias(std::size_t from, std::size_t to, std::string_view alias);

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
     * @brief Creates the entry of a normalised path and options, or returns the existing one.
     * @param path Normalised path.
     * @param options Load options.
     * @return Entry index.
     */
    std::size_t entryFor(const std::string& path, const LoadOptions& options);

    /**
     * @brief Frees an entry nothing declares nor holds; its slot is reused by the next new entry.
     * @param index Entry index.
     */
    void freeEntry(std::size_t index);

    /**
     * @brief Gets the handle served for an entry.
     * @param entry Entry.
     * @return Its handle, the fallback if it failed, or `Kind::NONE`.
     */
    [[nodiscard]] Id served(const Entry& entry) const noexcept {
        return entry.failed ? _fallback.value_or(Kind::NONE) : entry.id;
    }

    /** @brief State shared with the manager. */
    AssetContext* _context;
    /** @brief Entries; indices are stable while an entry is used, free slots are listed in `_free`. */
    std::vector<Entry> _entries;
    /** @brief Free slots of `_entries`. */
    std::vector<std::size_t> _free;
    /** @brief Entry of each path and options. */
    StringMap<std::size_t> _byKey;
    /** @brief Entries of each normalised path, one per set of options. */
    StringMap<std::vector<std::size_t>> _byPath;
    /** @brief Entry of each alias. */
    StringMap<std::size_t> _byAlias;
    /** @brief Entries each group declared. */
    StringMap<std::vector<std::size_t>> _byGroup;
    /** @brief Backend loader, if one is installed. */
    std::optional<AssetLoader<Kind>> _loader;
    /** @brief Fallback handle of the loader, once asked for. */
    std::optional<Id> _fallback;
};

}  // namespace assets::detail

#include "assets/detail/AssetCache.tpp"  // IWYU pragma: export
