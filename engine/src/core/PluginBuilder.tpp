/**
 * @file PluginBuilder.tpp
 * @brief Template implementations for PluginBuilder.
 */

#pragma once

#include <typeindex>

#include "core/PluginBuilder.hpp"  // NOLINT(misc-header-include-cycle)

namespace engine {

template <typename PluginType>
PluginBuilder& PluginBuilder::require() {
    _required.push_back(PluginDependency{
        .typeId = std::type_index(typeid(PluginType)),
        .name = typeid(PluginType).name(),
    });
    return *this;
}

template <typename PluginType>
PluginBuilder& PluginBuilder::optionally() {
    _optional.push_back(PluginDependency{
        .typeId = std::type_index(typeid(PluginType)),
        .name = typeid(PluginType).name(),
    });
    return *this;
}

}  // namespace engine
