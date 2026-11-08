#pragma once

#include <cstddef>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/detail/AssetCache.hpp"

namespace assets {

template <AssetKind Kind>
void AssetManager::registerKind() {
    static_cast<void>(cache<Kind>());
}

template <AssetKind Kind>
detail::AssetCache<Kind>& AssetManager::cache() {
    checkThread();
    if (const auto it = _byType.find(std::type_index(typeid(Kind))); it != _byType.end()) {
        return static_cast<detail::AssetCache<Kind>&>(*_caches[it->second]);
    }
    if (findCache(Kind::NAME) != nullptr) {
        throw std::logic_error("assets: another kind is already named '" + std::string(Kind::NAME) + "'");
    }
    _caches.push_back(std::make_unique<detail::AssetCache<Kind>>(_context));
    _byType.emplace(std::type_index(typeid(Kind)), _caches.size() - 1);
    return static_cast<detail::AssetCache<Kind>&>(*_caches.back());
}

template <AssetKind Kind>
const detail::AssetCache<Kind>* AssetManager::findCache() const {
    checkThread();
    const auto it = _byType.find(std::type_index(typeid(Kind)));
    return it != _byType.end() ? static_cast<const detail::AssetCache<Kind>*>(_caches[it->second].get()) : nullptr;
}

template <AssetKind Kind>
void AssetManager::setLoader(AssetLoader<Kind> loader) {
    cache<Kind>().setLoader(std::move(loader));
}

template <AssetKind Kind>
void AssetManager::detachLoader() {
    cache<Kind>().detachLoader();
}

template <AssetKind Kind>
bool AssetManager::hasLoader() const {
    const detail::AssetCache<Kind>* found = findCache<Kind>();
    return found != nullptr && found->hasLoader();
}

template <AssetKind Kind>
bool AssetManager::declare(const std::string_view alias, const std::string_view path, const std::string_view group,
                           const LoadOptions& options) {
    return cache<Kind>().declare(alias, path, group, options).has_value();
}

template <AssetKind Kind>
bool AssetManager::undeclare(const std::string_view alias) {
    return cache<Kind>().undeclare(alias);
}

template <AssetKind Kind>
Kind::Id AssetManager::load(const std::string_view aliasOrPath, const std::string_view group,
                            const LoadOptions& options) {
    return cache<Kind>().load(aliasOrPath, group, options);
}

template <AssetKind Kind>
Kind::Id AssetManager::get(const std::string_view aliasOrPath) const {
    const detail::AssetCache<Kind>* found = findCache<Kind>();
    return found != nullptr ? found->get(aliasOrPath) : Kind::NONE;
}

template <AssetKind Kind>
AssetStatus AssetManager::status(const std::string_view aliasOrPath) const {
    const detail::AssetCache<Kind>* found = findCache<Kind>();
    return found != nullptr ? found->statusOf(aliasOrPath) : AssetStatus::Unknown;
}

template <AssetKind Kind>
Kind::Id AssetManager::fallback() {
    return cache<Kind>().fallback();
}

template <AssetKind Kind>
std::size_t AssetManager::preloadDirectory(const std::filesystem::path& directory, const std::string_view group,
                                           const LoadOptions& options) {
    return cache<Kind>().preloadAny(directory, group, options);
}

template <AssetKind Kind>
std::size_t AssetManager::loadedCount() const {
    const detail::AssetCache<Kind>* found = findCache<Kind>();
    return found != nullptr ? found->loadedCount() : 0;
}

}  // namespace assets
