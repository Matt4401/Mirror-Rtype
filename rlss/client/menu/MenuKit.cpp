#include "menu/MenuKit.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "Arena.hpp"
#include "AssetsPlugin.hpp"
#include "components/Renderable.hpp"
#include "core/Engine.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "core/services/IWindowService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "protocol/Protocol.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiImage.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace rlss::client::menu {

namespace {

// Metrics of Russo One, as the render backend sizes fonts (same as `Typeface`).
/** @brief Height of the capital letters, as a share of the font size. */
constexpr float CAP_RATIO = 700.0F / 1205.0F;
/** @brief Gap between the top of a drawn text and the top of its capitals, as a share of the font size. */
constexpr float CAP_TOP_RATIO = 226.0F / 1205.0F;
/** @brief Size of the veil; larger than any window. */
constexpr float VEIL_SIZE = 8192.0F;
/** @brief Team-coloured strip on the left of the buttons. */
constexpr float BUTTON_ACCENT = 6.0F;

/**
 * @brief Gets the RLSS font.
 * @param context Scene context.
 * @return Font id, or the default font.
 */
engine::FontId fontOf(scene::SceneContext& context) {
    const ecs::Registry& registry = context.registry();
    return registry.hasResource<GameAssets>() ? registry.getResource<GameAssets>().font : engine::NO_FONT;
}

/**
 * @brief Spawns an element owned by the scene, as a child of `parent` (or on the screen).
 * @param context Scene owning it.
 * @param parent Parent, or `ecs::NULL_ENTITY`.
 * @param anchor Placement.
 * @return The element.
 */
ecs::Entity element(scene::SceneContext& context, ecs::Entity parent, const ui::UiAnchor& anchor) {
    ecs::Registry& registry = context.registry();
    const ecs::Entity entity = context.spawn();
    registry.addComponent(entity, anchor);
    if (parent != ecs::NULL_ENTITY) {
        hierarchy::setParent(registry, entity, parent);
    }
    return entity;
}

/**
 * @brief Spawns a filled rectangle.
 * @param context Scene owning it.
 * @param parent Parent, or `ecs::NULL_ENTITY`.
 * @param anchor Placement.
 * @param width Width.
 * @param height Height.
 * @param color Fill.
 * @return The rectangle.
 */
ecs::Entity rect(scene::SceneContext& context, ecs::Entity parent, const ui::UiAnchor& anchor, float width,
                 float height, ecs::Rgba color) {
    const ecs::Entity entity = element(context, parent, anchor);
    context.registry().addComponent(entity, ui::UiRect{.width = width, .height = height, .fillColor = color});
    return entity;
}

}  // namespace

ecs::Entity overlay(scene::SceneContext& context) {
    return rect(context, ecs::NULL_ENTITY, ui::UiAnchor{}, VEIL_SIZE, VEIL_SIZE, OVERLAY);
}

ecs::Entity text(scene::SceneContext& context, ecs::Entity parent, float anchorX, float anchorY, std::string text,
                 float cap, ecs::Rgba color, Align align, float offsetX) {
    const float size = cap / CAP_RATIO;
    float pivot = 0.5F;
    if (align == Align::Left) {
        pivot = 0.0F;
    } else if (align == Align::Right) {
        pivot = 1.0F;
    }
    const ecs::Entity entity = element(context, parent,
                                       ui::UiAnchor{.anchorX = anchorX,
                                                    .anchorY = anchorY,
                                                    .offsetX = offsetX,
                                                    .offsetY = -size * CAP_TOP_RATIO,
                                                    .pivotX = pivot});
    context.registry().addComponent(
        entity, ui::UiText{.text = std::move(text), .fontSize = size, .color = color, .fontId = fontOf(context)});
    return entity;
}

void title(scene::SceneContext& context, float anchorY, float cap) {
    const ecs::Registry& registry = context.registry();
    const float height = screenHeight(context);
    const ecs::TextureId band =
        registry.hasResource<GameAssets>() ? registry.getResource<GameAssets>().hudBand : ecs::NO_TEXTURE;
    // A soft glow behind, the name with its shadow, and a bar under it: blue on the left, orange on the right.
    const float glowWidth = cap * 9.0F;
    const float glowHeight = cap * 2.4F;
    const ecs::Entity glow = element(
        context, ecs::NULL_ENTITY,
        ui::UiAnchor{
            .anchorX = 0.5F, .anchorY = anchorY, .offsetY = (cap / 2.0F) - (glowHeight / 2.0F), .pivotX = 0.5F});
    context.registry().addComponent(
        glow,
        ui::UiImage{
            .texture = band, .width = glowWidth, .height = glowHeight, .tint = {.r = 40, .g = 110, .b = 255, .a = 70}});
    const float shadow = cap * 0.05F;
    const std::string name(protocol::GAME_TITLE);
    (void)text(context, ecs::NULL_ENTITY, 0.5F, anchorY + (shadow / height), name, cap,
               ecs::Rgba{.r = 0, .g = 0, .b = 0, .a = 140}, Align::Center, shadow);
    (void)text(context, ecs::NULL_ENTITY, 0.5F, anchorY, name, cap, TEXT);
    const float barWidth = textWidth(context, name, cap) * 0.55F;
    const float barTop = cap + (cap * 0.16F);
    const float barHeight = std::max(4.0F, cap * 0.06F);
    (void)rect(context, ecs::NULL_ENTITY,
               ui::UiAnchor{.anchorX = 0.5F, .anchorY = anchorY, .offsetX = -barWidth, .offsetY = barTop},
               barWidth - 3.0F, barHeight, opaque(BLUE_TEAM));
    (void)rect(context, ecs::NULL_ENTITY,
               ui::UiAnchor{.anchorX = 0.5F, .anchorY = anchorY, .offsetX = 3.0F, .offsetY = barTop}, barWidth - 3.0F,
               barHeight, opaque(ORANGE_TEAM));
}

ecs::Entity button(scene::SceneContext& context, ecs::Entity parent, float anchorX, float anchorY, float width,
                   float height, std::string label, std::string action, int order) {
    ecs::Registry& registry = context.registry();
    const ecs::Entity entity =
        element(context, parent, ui::UiAnchor{.anchorX = anchorX, .anchorY = anchorY, .pivotX = 0.5F});
    registry.addComponent(entity, ui::UiButton{.width = width,
                                               .height = height,
                                               .action = std::move(action),
                                               .colorIdle = BUTTON,
                                               .colorHovered = HOVER,
                                               .colorPressed = HOVER});
    registry.addComponent(entity, ui::UiFocusable{.order = order});
    const ecs::Entity edge = element(context, entity, ui::UiAnchor{});
    registry.addComponent(edge, ui::UiRect{.width = width,
                                           .height = height,
                                           .fillColor = {.r = 0, .g = 0, .b = 0, .a = 0},
                                           .borderColor = EDGE,
                                           .borderThickness = 1.0F});
    (void)rect(context, entity, ui::UiAnchor{}, BUTTON_ACCENT, height, opaque(BLUE_TEAM));
    if (!label.empty()) {
        // Centred on the part right of the accent strip, capitals centred vertically.
        const ecs::Entity caption =
            text(context, entity, 0.5F, 0.5F, std::move(label), LABEL_CAP, TEXT, Align::Center, BUTTON_ACCENT / 2.0F);
        registry.getComponents<ui::UiAnchor>().get(caption).offsetY -= LABEL_CAP / 2.0F;
    }
    return entity;
}

void focus(ecs::Registry& registry, ecs::Entity widget) {
    if (registry.hasResource<ui::UiFocus>()) {
        registry.getResource<ui::UiFocus>().focused = widget;
    }
}

void setText(ecs::Registry& registry, ecs::Entity element, std::string_view text) {
    if (registry.hasComponent<ui::UiText>(element)) {
        std::string& current = registry.getComponents<ui::UiText>().get(element).text;
        if (current != text) {
            current = text;
        }
    }
}

float textWidth(scene::SceneContext& context, std::string_view text, float cap) {
    const auto draw = context.engine().mediator().getService<engine::IScreenDrawService>();
    if (!draw || text.empty()) {
        return 0.0F;
    }
    const float size = cap / CAP_RATIO;
    return draw->get().measureText(text, size, size * 0.1F, fontOf(context)).width;
}

float screenHeight(scene::SceneContext& context) {
    const auto window = context.engine().mediator().getService<engine::IWindowService>();
    return window ? static_cast<float>(window->get().height()) : 720.0F;
}

float screenWidth(scene::SceneContext& context) {
    const auto window = context.engine().mediator().getService<engine::IWindowService>();
    return window ? static_cast<float>(window->get().width()) : 1280.0F;
}

void onBack(scene::SceneContext& context, std::function<void()> handler) {
    context.subscribe<ui::UiBackEvent>([&context, handler = std::move(handler)](const ui::UiBackEvent& /*event*/) {
        if (context.manager().top() == context.id()) {
            handler();
        }
    });
}

void onClick(scene::SceneContext& context, std::function<void(std::string_view action)> handler) {
    context.subscribe<ui::UiButtonClickedEvent>(
        [&context, handler = std::move(handler)](const ui::UiButtonClickedEvent& event) {
            if (context.manager().top() == context.id()) {
                handler(event.action);
            }
        });
}

}  // namespace rlss::client::menu
