/**
 * @file UiPlugin.cpp
 * @brief UiPlugin implementation.
 */

#include "ui/UiPlugin.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/system/UiButtonSystem.hpp"
#include "ui/system/UiInteractionSystem.hpp"
#include "ui/system/UiLayout.hpp"
#include "ui/system/UiRenderSystem.hpp"

namespace ui {

void UiPlugin::build(engine::PluginBuilder& builder) {
    builder.requireService<engine::IWindowService>();
    builder.requireService<engine::IScreenDrawService>();
    builder.require<hierarchy::HierarchyPlugin>();
}

void UiPlugin::init(engine::Engine& engine) {
    _uiRenderSystem = std::make_unique<UiRenderSystem>();
    ecs::Registry& registry = engine.registry();
    if (!registry.hasResource<UiFocus>()) {
        registry.emplaceResource<UiFocus>();
    }
    if (!registry.hasResource<UiNavigation>()) {
        registry.emplaceResource<UiNavigation>();
    }
}

UiScreenRect UiPlugin::screenOf(engine::Engine& engine) {
    const auto& window = engine.service<engine::IWindowService>();
    return UiScreenRect{.width = static_cast<float>(window.width()), .height = static_cast<float>(window.height())};
}

void UiPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    if (_uiRenderSystem == nullptr) {
        return;
    }
    const auto& draw = engine.service<engine::IScreenDrawService>();
    computeLayout(engine.registry(), screenOf(engine), UiRenderSystem::measureWith(draw), _layout);
    auto& window = engine.service<engine::IWindowService>();
    UiButtonSystem::update(engine.registry(), _layout, window, engine.eventBus());
    _interaction.update(engine.registry(), _layout, window, engine.eventBus(), time.deltaTime);
}

void UiPlugin::lateUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    if (_uiRenderSystem == nullptr) {
        return;
    }
    _uiRenderSystem->update(engine.registry(), screenOf(engine), engine.service<engine::IScreenDrawService>());
}

void UiPlugin::shutdown(engine::Engine& /*engine*/) {
    _uiRenderSystem.reset();
    _layout.clear();
    _layout.shrink_to_fit();
}

}  // namespace ui
