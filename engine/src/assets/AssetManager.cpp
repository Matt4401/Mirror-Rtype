#include "assets/AssetManager.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <tuple>

#include "assets/AssetKinds.hpp"
#include "assets/AssetPath.hpp"
#include "assets/detail/AssetCache.hpp"

namespace assets {

AssetManager::AssetManager()
    : _caches(detail::AssetCache<Texture>(_diagnostics), detail::AssetCache<Font>(_diagnostics),
              detail::AssetCache<Sound>(_diagnostics), detail::AssetCache<Music>(_diagnostics)) {}

bool AssetManager::loadGroup(const std::string_view group) {
    if (!isValidAssetName(group)) {
        _diagnostics.warn("invalid group name");
        return false;
    }
    if (!isGroupLoaded(group)) {
        _loadedGroups.emplace_back(group);
    }
    std::size_t missing = 0;
    std::apply([&](auto&... caches) { ((missing += caches.acquireDeclared(group)), ...); }, _caches);
    return missing == 0;
}

void AssetManager::unloadGroup(const std::string_view group) {
    std::erase(_loadedGroups, group);
    std::apply([&](auto&... caches) { (caches.release(group), ...); }, _caches);
}

bool AssetManager::isGroupLoaded(const std::string_view group) const {
    return std::ranges::find(_loadedGroups, group) != _loadedGroups.end();
}

void AssetManager::unloadAll() {
    _loadedGroups.clear();
    std::apply([](auto&... caches) { (caches.releaseAll(), ...); }, _caches);
}

}  // namespace assets
