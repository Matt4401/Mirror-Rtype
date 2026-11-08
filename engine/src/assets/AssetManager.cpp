#include "assets/AssetManager.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "assets/AssetKinds.hpp"
#include "assets/AssetPath.hpp"
#include "assets/detail/IAssetCache.hpp"

namespace assets {

AssetManager::AssetManager(std::filesystem::path root) : _owner(std::this_thread::get_id()) {
    _context.root = std::move(root);
    registerKind<Texture>();
    registerKind<Font>();
    registerKind<Sound>();
    registerKind<Music>();
}

bool AssetManager::hasKind(const std::string_view kind) const { return findCache(kind) != nullptr; }

std::vector<std::string_view> AssetManager::kinds() const {
    checkThread();
    std::vector<std::string_view> names;
    names.reserve(_caches.size());
    for (const auto& cache : _caches) {
        names.push_back(cache->kindName());
    }
    return names;
}

bool AssetManager::declare(const std::string_view kind, const std::string_view alias, const std::string_view path,
                           const std::string_view group, const LoadOptions& options) {
    detail::IAssetCache* cache = cacheOrWarn(kind);
    return cache != nullptr && cache->declareAny(alias, path, group, options);
}

bool AssetManager::undeclare(const std::string_view kind, const std::string_view alias) {
    detail::IAssetCache* cache = cacheOrWarn(kind);
    return cache != nullptr && cache->undeclare(alias);
}

std::uint64_t AssetManager::handle(const std::string_view kind, const std::string_view aliasOrPath) const {
    const detail::IAssetCache* cache = findCache(kind);
    return cache != nullptr ? cache->handleOf(aliasOrPath) : 0;
}

AssetStatus AssetManager::status(const std::string_view kind, const std::string_view aliasOrPath) const {
    const detail::IAssetCache* cache = findCache(kind);
    return cache != nullptr ? cache->statusOf(aliasOrPath) : AssetStatus::Unknown;
}

std::size_t AssetManager::preloadDirectory(const std::string_view kind, const std::filesystem::path& directory,
                                           const std::string_view group, const LoadOptions& options) {
    detail::IAssetCache* cache = cacheOrWarn(kind);
    return cache != nullptr ? cache->preloadAny(directory, group, options) : 0;
}

bool AssetManager::loadGroup(const std::string_view group) {
    const GroupProgress progress = loadGroupStep(group, UNLIMITED);
    return isValidAssetName(group) && progress.loaded == progress.total;
}

GroupProgress AssetManager::loadGroupStep(const std::string_view group, const std::size_t maxLoads) {
    checkThread();
    if (!isValidAssetName(group)) {
        _context.diagnostics.warn("invalid group name " + quoteForLog(group));
        return {};
    }
    if (!isGroupLoaded(group)) {
        _context.loadedGroups.emplace_back(group);
        for (const auto& cache : _caches) {
            cache->holdGroup(group);
        }
    }
    std::size_t budget = maxLoads;
    for (const auto& cache : _caches) {
        if (budget == 0) {
            break;
        }
        budget -= std::min(budget, cache->loadPending(group, budget));
    }
    return groupProgress(group);
}

GroupProgress AssetManager::groupProgress(const std::string_view group) const {
    checkThread();
    GroupProgress progress;
    for (const auto& cache : _caches) {
        cache->addProgress(group, progress);
    }
    return progress;
}

void AssetManager::unloadGroup(const std::string_view group) {
    checkThread();
    std::erase(_context.loadedGroups, group);
    for (const auto& cache : _caches) {
        cache->release(group);
    }
}

void AssetManager::forgetGroup(const std::string_view group) {
    unloadGroup(group);
    for (const auto& cache : _caches) {
        cache->forgetGroup(group);
    }
}

bool AssetManager::isGroupLoaded(const std::string_view group) const {
    checkThread();
    return _context.isGroupLoaded(group);
}

void AssetManager::unloadAll() {
    checkThread();
    _context.loadedGroups.clear();
    for (const auto& cache : _caches) {
        cache->releaseAll();
    }
}

void AssetManager::beginScriptDeclarations(const bool replacing) {
    checkThread();
    if (replacing) {
        for (const auto& cache : _caches) {
            cache->markScriptDeclarationsStale();
        }
    }
    _context.scriptWindow = true;
}

void AssetManager::endScriptDeclarations(const bool dropStale) {
    checkThread();
    _context.scriptWindow = false;
    for (const auto& cache : _caches) {
        if (dropStale) {
            cache->dropStaleDeclarations();
        } else {
            cache->keepStaleDeclarations();
        }
    }
}

detail::IAssetCache* AssetManager::findCache(const std::string_view kind) const {
    checkThread();
    const auto it = std::ranges::find_if(_caches, [kind](const auto& cache) { return cache->kindName() == kind; });
    return it != _caches.end() ? it->get() : nullptr;
}

detail::IAssetCache* AssetManager::cacheOrWarn(const std::string_view kind) {
    checkThread();
    if (!hasKind(kind)) {
        _context.diagnostics.warnOnce("kind:" + std::string(kind), "unknown asset kind " + quoteForLog(kind));
        return nullptr;
    }
    return findCache(kind);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): reads `_owner` in debug builds.
void AssetManager::checkThread() const noexcept {
    assert(std::this_thread::get_id() == _owner && "assets::AssetManager is used from another thread");
}

}  // namespace assets
