/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** UiPlugin implementation.
*/

#include "ui/UiPlugin.hpp"

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "render/RenderPlugin.hpp"

namespace ui {

void UiPlugin::build(engine::PluginBuilder& builder) { builder.require<render::RenderPlugin>(); }

void UiPlugin::init(engine::Engine& /*engine*/) {
    _fonts = std::make_unique<FontStore>();
    _fonts->loadDefault();
    _uiRenderSystem = std::make_unique<UiRenderSystem>(*_fonts);
}

void UiPlugin::render(engine::Engine& engine) {
    if (_uiRenderSystem != nullptr) {
        _uiRenderSystem->update(engine.registry());
    }
}

}  // namespace ui
