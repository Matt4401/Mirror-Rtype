/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** UiPlugin implementation.
*/

#include "ui/UiPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/time/Time.hpp"
#include "render/RenderPlugin.hpp"

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
    _buttonSystem.update(engine.registry(), engine.service<engine::IWindowService>(), engine.eventBus());
}

void UiPlugin::render(engine::Engine& engine) {
    if (_uiRenderSystem != nullptr) {
        _uiRenderSystem->update(engine.registry());
    }
}

}  // namespace ui
