/**
 * @file IAssetCache.hpp
 * @brief Interface of the cache of one kind of asset, through which the manager drives every kind together and
 * scripts reach a kind by its name.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string_view>

#include "assets/AssetKinds.hpp"

namespace assets::detail {

/** @brief Kind-independent operations of an `AssetCache`; handles are widened to 64 bits. */
class IAssetCache {
  public:
    /** @brief Virtual destructor. */
    virtual ~IAssetCache() = default;

    /** @brief Default constructor. */
    IAssetCache() = default;
    /** @brief Deleted copy constructor. */
    IAssetCache(const IAssetCache&) = delete;
    /** @brief Deleted copy assignment. */
    IAssetCache& operator=(const IAssetCache&) = delete;
    /** @brief Deleted move constructor. */
    IAssetCache(IAssetCache&&) = delete;
    /** @brief Deleted move assignment. */
    IAssetCache& operator=(IAssetCache&&) = delete;

    /**
     * @brief Gets the name of the kind.
     * @return `Kind::NAME`.
     */
    [[nodiscard]] virtual std::string_view kindName() const noexcept = 0;

    /**
     * @brief Records that `group` uses a file, under `alias` if not empty; loads it if the group is loaded.
     * @param alias Alias, or empty.
     * @param path File path.
     * @param group Group.
     * @param options Load options.
     * @return False for an invalid alias, path or group (warning).
     */
    virtual bool declareAny(std::string_view alias, std::string_view path, std::string_view group,
                            const LoadOptions& options) = 0;

    /**
     * @brief Removes every declaration of an alias; what no group needs any more is released.
     * @param alias Alias.
     * @return False if the alias is unknown.
     */
    virtual bool undeclare(std::string_view alias) = 0;

    /**
     * @brief Declares the matching files of a directory.
     * @param directory Directory.
     * @param group Group.
     * @param options Load options.
     * @return Number of files declared.
     */
    virtual std::size_t preloadAny(const std::filesystem::path& directory, std::string_view group,
                                   const LoadOptions& options) = 0;

    /**
     * @brief Gets the handle served for an alias or a path.
     * @param aliasOrPath Alias or path.
     * @return The handle, the fallback of a failed asset, or 0.
     */
    [[nodiscard]] virtual std::uint64_t handleOf(std::string_view aliasOrPath) const = 0;

    /**
     * @brief Gets where an asset stands.
     * @param aliasOrPath Alias or path.
     * @return Its status.
     */
    [[nodiscard]] virtual AssetStatus statusOf(std::string_view aliasOrPath) const = 0;

    /**
     * @brief Makes `group` hold every entry it declared, without loading anything.
     * @param group Group.
     */
    virtual void holdGroup(std::string_view group) = 0;

    /**
     * @brief Loads held entries of `group` that are not loaded yet.
     * @param group Group.
     * @param budget Most loads to make.
     * @return Loads made (failed ones included).
     */
    virtual std::size_t loadPending(std::string_view group, std::size_t budget) = 0;

    /**
     * @brief Adds the state of the entries of `group` to `progress`.
     * @param group Group.
     * @param progress Progress to add to.
     */
    virtual void addProgress(std::string_view group, GroupProgress& progress) const = 0;

    /**
     * @brief Makes `group` let every entry go, releasing the ones no other group holds.
     * @param group Group.
     */
    virtual void release(std::string_view group) = 0;

    /**
     * @brief Removes every declaration of `group`, then releases what it held.
     * @param group Group.
     */
    virtual void forgetGroup(std::string_view group) = 0;

    /** @brief Releases every loaded entry; no group holds anything afterwards. */
    virtual void releaseAll() = 0;

    /** @brief Marks the declarations made by scripts as stale, before a hot reload runs them again. */
    virtual void markScriptDeclarationsStale() = 0;

    /** @brief Removes the declarations still stale after the hot reload: the script no longer makes them. */
    virtual void dropStaleDeclarations() = 0;
};

}  // namespace assets::detail
