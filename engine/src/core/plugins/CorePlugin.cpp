/**
 * @file CorePlugin.cpp
 * @brief Implementation of the implicit core plugin.
 */

#include "core/plugins/CorePlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"

namespace engine {

void CorePlugin::build(PluginBuilder& builder) { (void)builder; }

void CorePlugin::init(Engine& engine) { (void)engine; }

void CorePlugin::shutdown(Engine& engine) { (void)engine; }

}  // namespace engine
