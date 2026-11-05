/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** UiPlugin implementation.
*/

#include "ui/UiPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/time/Time.hpp"
#include "render/RenderPlugin.hpp"
#include "ui/font/FontStore.hpp"
#include "ui/system/UiButtonSystem.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace ui {

void UiPlugin::build(engine::PluginBuilder& builder) {
    builder.require<render::RenderPlugin>();
    builder.requireService<engine::IWindowService>();
}

void UiPlugin::init(engine::Engine& /*engine*/) {
    _fonts = std::make_unique<FontStore>();
    _fonts->loadDefault();
    _uiRenderSystem = std::make_unique<UiRenderSystem>(*_fonts);
}

void UiPlugin::update(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    UiButtonSystem::update(engine.registry(), engine.service<engine::IWindowService>(), engine.eventBus());
}

void UiPlugin::render(engine::Engine& engine) {
    if (_uiRenderSystem != nullptr) {
        const auto& window = engine.service<engine::IWindowService>();
        _uiRenderSystem->update(engine.registry(), window.width(), window.height());
    }
}

}  // namespace ui
