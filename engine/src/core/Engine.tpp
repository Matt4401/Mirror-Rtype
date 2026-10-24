/**
 * @file Engine.tpp
 * @brief Template implementations for Engine.
 */

#pragma once

#include <memory>
#include <typeindex>
#include <utility>

#include "core/Engine.hpp"  // NOLINT(misc-header-include-cycle)
#include "core/IPlugin.hpp"

namespace engine {

template <typename Plugin, typename... Args>
Engine& Engine::addPlugin(Args&&... args) {
    auto plugin = std::make_unique<Plugin>(std::forward<Args>(args)...);
    const std::type_index typeId(typeid(Plugin));
    _pluginGraph.addPlugin(std::move(plugin), typeId);
    return *this;
}

template <typename Plugin>
bool Engine::hasPlugin() const noexcept {
    const std::type_index typeId(typeid(Plugin));
    return _pluginGraph.hasPlugin(typeId);
}

template <typename Plugin>
Plugin& Engine::getPlugin() {
    const std::type_index typeId(typeid(Plugin));
    return static_cast<Plugin&>(_pluginGraph.getPlugin(typeId));
}

}  // namespace engine
