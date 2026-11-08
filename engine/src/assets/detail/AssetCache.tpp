#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetPath.hpp"
#include "assets/detail/AssetCache.hpp"

namespace assets::detail {

namespace cache_detail {

/**
 * @brief Adds a value to a list if it is not in it yet.
 * @param values List.
 * @param value Value to add.
 */
template <typename T, typename U>
void addUnique(std::vector<T>& values, const U& value) {
    if (std::ranges::find(values, value) == values.end()) {
        values.emplace_back(value);
    }
}

/**
 * @brief Removes a value from a list.
 * @param values List.
 * @param value Value to remove.
 */
template <typename T, typename U>
void removeValue(std::vector<T>& values, const U& value) {
    std::erase(values, value);
}

/**
 * @brief Builds the key of an entry: its path and its options.
 * @param path Normalised path.
 * @param options Load options.
 * @return Key.
 */
inline std::string entryKey(const std::string& path, const LoadOptions& options) {
    return path + '\x1f' + std::to_string(static_cast<int>(options.filter));
}

/**
 * @brief Tells whether a file has one of the extensions of a kind, whatever their case.
 * @tparam Kind Kind of asset.
 * @param file File path.
 * @return True for a matching extension.
 */
template <AssetKind Kind>
[[nodiscard]] bool hasExtensionOf(const std::filesystem::path& file) {
    std::string extension = toUtf8(file.extension());
    std::ranges::transform(extension, extension.begin(), [](const unsigned char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : static_cast<char>(c);
    });
    return std::ranges::find(Kind::EXTENSIONS, std::string_view(extension)) != Kind::EXTENSIONS.end();
}

}  // namespace cache_detail

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::declare(const std::string_view alias, const std::string_view path,
                                                     const std::string_view group, const LoadOptions& options) {
    const std::optional<std::string> normalized = normalizeAssetPath(path);
    if (!normalized) {
        _context->diagnostics.warn(std::string(Kind::NAME) + " " + quoteForLog(alias) + ": invalid path " +
                                   quoteForLog(path));
        return std::nullopt;
    }
    if ((!alias.empty() && !isValidAssetAlias(alias)) || !isValidAssetName(group)) {
        _context->diagnostics.warn(std::string(Kind::NAME) + " " + quoteForLog(*normalized, MAX_PATH_LENGTH) +
                                   ": invalid alias " + quoteForLog(alias) + " or group " + quoteForLog(group) +
                                   " (an alias holds no '/', '\\' or '.')");
        return std::nullopt;
    }
    const std::size_t index = entryFor(*normalized, options);
    if (!alias.empty()) {
        if (const auto it = _byAlias.find(alias); it == _byAlias.end()) {
            _byAlias.emplace(std::string(alias), index);
        } else if (it->second != index) {
            const std::size_t previous = it->second;
            _context->diagnostics.warn(std::string(Kind::NAME) + " alias " + quoteForLog(alias) + " now names " +
                                       *normalized + " instead of " + _entries[previous].path);
            it->second = index;
            moveAlias(previous, index, alias);
        }
    }
    addDeclaration(index, alias, group);
    if (_context->isGroupLoaded(group)) {
        acquire(index, group);
    }
    return index;
}

template <AssetKind Kind>
AssetCache<Kind>::Id AssetCache<Kind>::load(const std::string_view aliasOrPath, const std::string_view group,
                                            const LoadOptions& options) {
    if (!isValidAssetName(group)) {
        _context->diagnostics.warn(std::string(Kind::NAME) + " " + quoteForLog(aliasOrPath) + ": invalid group " +
                                   quoteForLog(group));
        return Kind::NONE;
    }
    std::optional<std::size_t> index;
    if (looksLikeAssetPath(aliasOrPath)) {
        index = declare({}, aliasOrPath, group, options);
    } else if (const auto it = _byAlias.find(aliasOrPath); it != _byAlias.end()) {
        index = it->second;
        // Under the alias, so that undeclaring or rebinding it also takes this group's hold away.
        addDeclaration(*index, aliasOrPath, group);
    } else {
        // Not a path (no '/', '\' or '.'): a typo in an alias, never a file to look for on the disk.
        _context->diagnostics.warnOnce(
            std::string(Kind::NAME) + ':' + std::string(aliasOrPath),
            std::string(Kind::NAME) + " alias " + quoteForLog(aliasOrPath) + " is not declared");
        return Kind::NONE;
    }
    if (!index) {
        return Kind::NONE;
    }
    acquire(*index, group);
    return served(_entries[*index]);
}

template <AssetKind Kind>
AssetCache<Kind>::Id AssetCache<Kind>::get(const std::string_view aliasOrPath) const {
    const std::optional<std::size_t> index = find(aliasOrPath);
    return index ? served(_entries[*index]) : Kind::NONE;
}

template <AssetKind Kind>
AssetCache<Kind>::Id AssetCache<Kind>::fallback() {
    if (!_fallback && _loader && _loader->fallback) {
        _fallback = _loader->fallback();
    }
    return _fallback.value_or(Kind::NONE);
}

template <AssetKind Kind>
AssetStatus AssetCache<Kind>::statusOf(const std::string_view aliasOrPath) const {
    const std::optional<std::size_t> index = find(aliasOrPath);
    if (!index) {
        return AssetStatus::Unknown;
    }
    const Entry& entry = _entries[*index];
    if (entry.failed) {
        return AssetStatus::Failed;
    }
    return entry.id != Kind::NONE ? AssetStatus::Loaded : AssetStatus::Declared;
}

template <AssetKind Kind>
bool AssetCache<Kind>::undeclare(const std::string_view alias) {
    const auto it = _byAlias.find(alias);
    if (it == _byAlias.end()) {
        return false;
    }
    const std::size_t index = it->second;
    _byAlias.erase(it);
    removeDeclarations(index, [alias](const Declaration& declaration) { return declaration.alias == alias; });
    return true;
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::preloadAny(const std::filesystem::path& directory, const std::string_view group,
                                         const LoadOptions& options) {
    const std::string shown = toUtf8(directory);
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        _context->diagnostics.warn(std::string(Kind::NAME) + " directory " + quoteForLog(shown, MAX_PATH_LENGTH) +
                                   " not found");
        return 0;
    }
    std::vector<std::filesystem::path> files;
    for (auto it = std::filesystem::directory_iterator(directory, error);
         !error && it != std::filesystem::directory_iterator(); it.increment(error)) {
        std::error_code typeError;
        if (it->is_regular_file(typeError) && cache_detail::hasExtensionOf<Kind>(it->path())) {
            files.push_back(it->path());
        }
    }
    if (error) {
        _context->diagnostics.warn(std::string(Kind::NAME) + " directory " + quoteForLog(shown, MAX_PATH_LENGTH) +
                                   ": listing stopped early (" + error.message() + ")");
    }
    std::ranges::sort(files);
    if (files.size() > MAX_DIRECTORY_FILES) {
        _context->diagnostics.warn(shown + ": only the first " + std::to_string(MAX_DIRECTORY_FILES) +
                                   " files are taken");
        files.resize(MAX_DIRECTORY_FILES);
    }
    std::size_t taken = 0;
    for (const std::filesystem::path& file : files) {
        const std::string alias = toUtf8(file.stem());
        const std::string path = toUtf8(file);
        if (const std::optional<std::size_t> owner = liveAliasOwner(alias)) {
            const std::optional<std::string> normalized = normalizeAssetPath(path);
            if (normalized && _entries[*owner].key != cache_detail::entryKey(*normalized, options)) {
                // "ship.jpg" next to "ship.png", or an alias another declaration uses: never steal it.
                _context->diagnostics.warn(std::string(Kind::NAME) + " " + quoteForLog(path, MAX_PATH_LENGTH) +
                                           ": alias " + quoteForLog(alias) + " already names " + _entries[*owner].path +
                                           ", file skipped");
                continue;
            }
        }
        if (declare(alias, path, group, options)) {
            ++taken;
        }
    }
    return taken;
}

template <AssetKind Kind>
void AssetCache<Kind>::holdGroup(const std::string_view group) {
    const auto it = _byGroup.find(group);
    if (it == _byGroup.end()) {
        return;
    }
    for (const std::size_t index : it->second) {
        cache_detail::addUnique(_entries[index].holders, group);
    }
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::loadPending(const std::string_view group, const std::size_t budget) {
    const auto it = _byGroup.find(group);
    if (it == _byGroup.end() || !_loader) {
        return 0;
    }
    std::size_t made = 0;
    for (const std::size_t index : it->second) {
        if (made >= budget) {
            break;
        }
        Entry& entry = _entries[index];
        if (entry.id == Kind::NONE && !entry.failed && !entry.holders.empty()) {
            ensureLoaded(entry);
            ++made;
        }
    }
    return made;
}

template <AssetKind Kind>
void AssetCache<Kind>::addProgress(const std::string_view group, GroupProgress& progress) const {
    const auto it = _byGroup.find(group);
    if (it == _byGroup.end()) {
        return;
    }
    for (const std::size_t index : it->second) {
        const Entry& entry = _entries[index];
        ++progress.total;
        if (entry.id != Kind::NONE) {
            ++progress.loaded;
        } else if (entry.failed) {
            ++progress.failed;
        } else if (!_loader) {
            ++progress.waiting;
        }
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::release(const std::string_view group) {
    const auto it = _byGroup.find(group);
    if (it == _byGroup.end()) {
        return;
    }
    for (const std::size_t index : it->second) {
        dropHold(index, group);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::forgetGroup(const std::string_view group) {
    const auto it = _byGroup.find(group);
    if (it == _byGroup.end()) {
        return;
    }
    const std::vector<std::size_t> indices = it->second;
    for (const std::size_t index : indices) {
        removeDeclarations(index, [group](const Declaration& declaration) { return declaration.group == group; });
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
void AssetCache<Kind>::markScriptDeclarationsStale() {
    for (Entry& entry : _entries) {
        for (Declaration& declaration : entry.declarations) {
            declaration.stale = declaration.byScript;
        }
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::dropStaleDeclarations() {
    for (std::size_t index = 0; index < _entries.size(); ++index) {
        if (!_entries[index].used) {
            continue;
        }
        // Only the script's part goes: a declaration C++ also made stays.
        for (Declaration& declaration : _entries[index].declarations) {
            if (declaration.stale) {
                declaration.byScript = false;
                declaration.stale = false;
            }
        }
        removeDeclarations(index,
                           [](const Declaration& declaration) { return !declaration.byCode && !declaration.byScript; });
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::keepStaleDeclarations() {
    for (Entry& entry : _entries) {
        for (Declaration& declaration : entry.declarations) {
            declaration.stale = false;
        }
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::setLoader(AssetLoader<Kind> loader) {
    if (_loader) {
        detachLoader();
    }
    _loader.emplace(std::move(loader));
    if (_loader->reserve) {
        Id last = _highestHandle;
        for (const Entry& entry : _entries) {
            last = std::max(last, entry.lastId);
        }
        if (last != Kind::NONE) {
            _loader->reserve(last);
        }
    }
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
    _fallback.reset();
}

template <AssetKind Kind>
std::size_t AssetCache<Kind>::loadedCount() const noexcept {
    return static_cast<std::size_t>(
        std::ranges::count_if(_entries, [](const Entry& entry) { return entry.id != Kind::NONE; }));
}

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::find(const std::string_view aliasOrPath) const {
    if (!looksLikeAssetPath(aliasOrPath)) {
        const auto it = _byAlias.find(aliasOrPath);
        return it != _byAlias.end() ? std::optional<std::size_t>(it->second) : std::nullopt;
    }
    const std::optional<std::string> normalized = normalizeAssetPath(aliasOrPath);
    if (!normalized) {
        return std::nullopt;
    }
    const auto it = _byPath.find(*normalized);
    if (it == _byPath.end() || it->second.empty()) {
        return std::nullopt;
    }
    // One entry per set of options: the loaded one is the most useful answer.
    for (const std::size_t index : it->second) {
        if (_entries[index].id != Kind::NONE) {
            return index;
        }
    }
    return it->second.front();
}

template <AssetKind Kind>
std::optional<std::size_t> AssetCache<Kind>::liveAliasOwner(const std::string_view alias) const {
    const auto it = _byAlias.find(alias);
    if (it == _byAlias.end()) {
        return std::nullopt;
    }
    const bool live = std::ranges::any_of(_entries[it->second].declarations, [alias](const Declaration& declaration) {
        return declaration.alias == alias && !declaration.stale;
    });
    return live ? std::optional<std::size_t>(it->second) : std::nullopt;
}

template <AssetKind Kind>
void AssetCache<Kind>::acquire(const std::size_t index, const std::string_view group) {
    Entry& entry = _entries[index];
    cache_detail::addUnique(entry.holders, group);
    ensureLoaded(entry);
}

template <AssetKind Kind>
void AssetCache<Kind>::dropHold(const std::size_t index, const std::string_view group) {
    Entry& entry = _entries[index];
    const auto it = std::ranges::find(entry.holders, group);
    if (it == entry.holders.end()) {
        return;
    }
    entry.holders.erase(it);
    if (entry.holders.empty()) {
        unloadEntry(entry);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::addDeclaration(const std::size_t index, const std::string_view alias,
                                      const std::string_view group) {
    Entry& entry = _entries[index];
    const bool byScript = _context->scriptWindow;
    const auto it = std::ranges::find_if(entry.declarations, [&](const Declaration& declaration) {
        return declaration.alias == alias && declaration.group == group;
    });
    if (it != entry.declarations.end()) {
        if (byScript) {
            it->byScript = true;
            it->stale = false;
        } else {
            it->byCode = true;
        }
    } else {
        entry.declarations.push_back(Declaration{.alias = std::string(alias),
                                                 .group = std::string(group),
                                                 .byCode = !byScript,
                                                 .byScript = byScript,
                                                 .stale = false});
    }
    cache_detail::addUnique(_byGroup[std::string(group)], index);
}

template <AssetKind Kind>
template <typename Predicate>
void AssetCache<Kind>::removeDeclarations(const std::size_t index, const Predicate& predicate) {
    Entry& entry = _entries[index];
    std::vector<std::string> touchedGroups;
    std::vector<std::string> touchedAliases;
    for (const Declaration& declaration : entry.declarations) {
        if (predicate(declaration)) {
            cache_detail::addUnique(touchedGroups, declaration.group);
            if (!declaration.alias.empty()) {
                cache_detail::addUnique(touchedAliases, declaration.alias);
            }
        }
    }
    if (touchedGroups.empty()) {
        return;
    }
    std::erase_if(entry.declarations, predicate);
    for (const std::string& alias : touchedAliases) {
        const bool stillNamed = std::ranges::any_of(
            entry.declarations, [&](const Declaration& declaration) { return declaration.alias == alias; });
        if (const auto it = _byAlias.find(alias); !stillNamed && it != _byAlias.end() && it->second == index) {
            _byAlias.erase(it);
        }
    }
    for (const std::string& group : touchedGroups) {
        const bool stillDeclared = std::ranges::any_of(
            entry.declarations, [&](const Declaration& declaration) { return declaration.group == group; });
        if (stillDeclared) {
            continue;
        }
        if (const auto it = _byGroup.find(group); it != _byGroup.end()) {
            cache_detail::removeValue(it->second, index);
            if (it->second.empty()) {
                _byGroup.erase(it);
            }
        }
        dropHold(index, group);
    }
    if (_entries[index].declarations.empty()) {
        freeEntry(index);
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::moveAlias(const std::size_t from, const std::size_t to, const std::string_view alias) {
    std::vector<Declaration> moved;
    for (const Declaration& declaration : _entries[from].declarations) {
        if (declaration.alias == alias) {
            moved.push_back(declaration);
        }
    }
    const Id previousHandle = _entries[from].lastId;
    // Holds made by `load` in a group that is not "loaded" move too.
    const std::vector<std::string> previousHolders = _entries[from].holders;
    removeDeclarations(from, [alias](const Declaration& declaration) { return declaration.alias == alias; });
    // The old file is gone: the new one takes its handle, so the entities and C++ code that kept it now show the new
    // file instead of nothing (hot reload of a path).
    Entry& target = _entries[to];
    if (!_entries[from].used && target.id == Kind::NONE && target.lastId == Kind::NONE) {
        target.lastId = previousHandle;
    }
    // Declarations move with their ownership and staleness: a group the script no longer names stays stale.
    for (Declaration& declaration : moved) {
        Entry& entry = _entries[to];
        const auto it = std::ranges::find_if(entry.declarations, [&](const Declaration& existing) {
            return existing.alias == declaration.alias && existing.group == declaration.group;
        });
        if (it != entry.declarations.end()) {
            it->byCode = it->byCode || declaration.byCode;
            it->byScript = it->byScript || declaration.byScript;
            it->stale = it->stale && declaration.stale;
        } else {
            entry.declarations.push_back(declaration);
        }
        cache_detail::addUnique(_byGroup[declaration.group], to);
        if (_context->isGroupLoaded(declaration.group) ||
            std::ranges::find(previousHolders, declaration.group) != previousHolders.end()) {
            acquire(to, declaration.group);
        }
    }
}

template <AssetKind Kind>
void AssetCache<Kind>::ensureLoaded(Entry& entry) {
    if (entry.holders.empty() || entry.id != Kind::NONE || entry.failed || !_loader || !_loader->load) {
        return;
    }
    entry.id = _loader->load(entry.path, entry.options, entry.lastId);
    if (entry.id == Kind::NONE) {
        entry.failed = true;
        fallback();
        _context->diagnostics.warn("cannot load " + std::string(Kind::NAME) + " " +
                                   quoteForLog(entry.path, MAX_PATH_LENGTH) +
                                   (_fallback.value_or(Kind::NONE) != Kind::NONE ? ": drawn as the fallback" : ""));
        return;
    }
    entry.lastId = entry.id;
    _highestHandle = std::max(_highestHandle, entry.id);
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
std::size_t AssetCache<Kind>::entryFor(const std::string& path, const LoadOptions& options) {
    std::string key = cache_detail::entryKey(path, options);
    if (const auto it = _byKey.find(key); it != _byKey.end()) {
        return it->second;
    }
    std::size_t index = _entries.size();
    if (!_free.empty()) {
        index = _free.back();
        _free.pop_back();
    } else {
        _entries.emplace_back();
    }
    _entries[index] = Entry{.used = true,
                            .path = path,
                            .key = key,
                            .options = options,
                            .id = Kind::NONE,
                            .lastId = Kind::NONE,
                            .failed = false,
                            .declarations = {},
                            .holders = {}};
    _byKey.emplace(std::move(key), index);
    _byPath[path].push_back(index);
    return index;
}

template <AssetKind Kind>
void AssetCache<Kind>::freeEntry(const std::size_t index) {
    Entry& entry = _entries[index];
    entry.holders.clear();
    unloadEntry(entry);
    _byKey.erase(entry.key);
    if (const auto it = _byPath.find(entry.path); it != _byPath.end()) {
        cache_detail::removeValue(it->second, index);
        if (it->second.empty()) {
            _byPath.erase(it);
        }
    }
    entry = Entry{};
    _free.push_back(index);
}

}  // namespace assets::detail
