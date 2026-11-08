#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "assets/AssetKinds.hpp"
#include "assets/AssetPath.hpp"
#include "assets/detail/AssetCache.hpp"

namespace assets::detail {

namespace cache_detail {

/**
 * @brief Adds a group to a list if it is not in it yet.
 * @param groups List of groups.
 * @param group Group to add.
 */
inline void addGroup(std::vector<std::string>& groups, const std::string_view group) {
    if (std::ranges::find(groups, group) == groups.end()) {
        groups.emplace_back(group);
    }
}

}  // namespace cache_detail

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::declare(const std::string_view alias, const std::string_view path,
                                                     const std::string_view group, const LoadOptions& options) {
    const std::optional<std::string> key = normalizeAssetPath(path);
    if (!key) {
        _diagnostics->warn(std::string(Kind::NAME) + " '" + std::string(alias) + "': invalid path");
        return std::nullopt;
    }
    if ((!alias.empty() && !isValidAssetName(alias)) || !isValidAssetName(group)) {
        _diagnostics->warn(std::string(Kind::NAME) + " " + *key + ": invalid alias or group name");
        return std::nullopt;
    }
    const std::size_t index = entryFor(*key, options);
    if (_entries.at(index).options != options) {
        _diagnostics->warn(std::string(Kind::NAME) + " " + *key +
                           " declared again with other options: the first ones are kept");
    }
    if (!alias.empty()) {
        if (const auto it = _byAlias.find(alias); it == _byAlias.end()) {
            _byAlias.emplace(std::string(alias), index);
        } else if (it->second != index) {
            _diagnostics->warn(std::string(Kind::NAME) + " alias '" + std::string(alias) + "' now names " + *key +
                               " instead of " + _entries.at(it->second).path);
            it->second = index;
        }
    }
    cache_detail::addGroup(_entries.at(index).declaredIn, group);
    return index;
}

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::find(const std::string_view aliasOrPath) const {
    if (const auto it = _byAlias.find(aliasOrPath); it != _byAlias.end()) {
        return it->second;
    }
    const std::optional<std::string> key = normalizeAssetPath(aliasOrPath);
    if (!key) {
        return std::nullopt;
    }
    if (const auto it = _byPath.find(*key); it != _byPath.end()) {
        return it->second;
    }
    return std::nullopt;
}

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::findOrCreate(const std::string_view aliasOrPath,
                                                          const LoadOptions& options) {
    if (const std::optional<std::size_t> found = find(aliasOrPath)) {
        return found;
    }
    const std::optional<std::string> key = normalizeAssetPath(aliasOrPath);
    if (!key) {
        _diagnostics->warn(std::string(Kind::NAME) + ": invalid path");
        return std::nullopt;
    }
    return entryFor(*key, options);
}

template <AssetKind Kind>
AssetCache<Kind>::Id AssetCache<Kind>::acquire(const std::size_t entry, const std::string_view group) {
    Entry& target = _entries.at(entry);
    cache_detail::addGroup(target.holders, group);
    ensureLoaded(target);
    return target.id;
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::acquireDeclared(const std::string_view group) {
    std::size_t missing = 0;
    for (std::size_t i = 0; i < _entries.size(); ++i) {
        const std::vector<std::string>& declaredIn = _entries.at(i).declaredIn;
        if (std::ranges::find(declaredIn, group) != declaredIn.end() && acquire(i, group) == Kind::NONE) {
            ++missing;
        }
    }
    return missing;
}

template <AssetKind Kind>
void AssetCache<Kind>::release(const std::string_view group) {
    for (Entry& entry : _entries) {
        const auto it = std::ranges::find(entry.holders, group);
        if (it == entry.holders.end()) {
            continue;
        }
        entry.holders.erase(it);
        if (entry.holders.empty()) {
            unloadEntry(entry);
        }
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::releaseAll() {
    for (Entry& entry : _entries) {
        entry.holders.clear();
        unloadEntry(entry);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::setLoader(AssetLoader<Kind> loader) {
    if (_loader) {
        detachLoader();
    }
    _loader.emplace(std::move(loader));
    for (Entry& entry : _entries) {
        ensureLoaded(entry);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::detachLoader() {
    for (Entry& entry : _entries) {
        unloadEntry(entry);
    }
    _loader.reset();
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::loadedCount() const noexcept {
    return static_cast<std::size_t>(
        std::ranges::count_if(_entries, [](const Entry& entry) { return entry.id != Kind::NONE; }));
}

template <AssetKind Kind>
void AssetCache<Kind>::ensureLoaded(Entry& entry) {
    if (entry.holders.empty() || entry.id != Kind::NONE || entry.failed || !_loader || !_loader->load) {
        return;
    }
    entry.id = _loader->load(entry.path, entry.options);
    if (entry.id == Kind::NONE) {
        entry.failed = true;
        _diagnostics->warn("cannot load " + std::string(Kind::NAME) + " " + entry.path);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::unloadEntry(Entry& entry) {
    if (entry.id != Kind::NONE && _loader && _loader->unload) {
        _loader->unload(entry.id);
    }
    entry.id = Kind::NONE;
    entry.failed = false;
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::entryFor(const std::string& key, const LoadOptions& options) {
    if (const auto it = _byPath.find(key); it != _byPath.end()) {
        return it->second;
    }
    const std::size_t index = _entries.size();
    _entries.push_back(
        Entry{.path = key, .options = options, .id = Kind::NONE, .failed = false, .declaredIn = {}, .holders = {}});
    _byPath.emplace(key, index);
    return index;
}

}  // namespace assets::detail
