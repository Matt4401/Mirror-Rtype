#include "scenes/MenuKit.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "components/Renderable.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/UiEvents.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"

namespace client::menu {

namespace {

/** @brief Horizontal room for the caption of a slider or a toggle. */
constexpr float CAPTION_WIDTH = 170.0F;
/** @brief Font size of the caption of a slider or a toggle. */
constexpr float CAPTION_SIZE = 20.0F;
/** @brief Width of a settings row: caption and widget. */
constexpr float ROW_WIDTH = 400.0F;
/** @brief Button background when idle. */
constexpr ecs::Rgba BUTTON_IDLE = {.r = 32, .g = 40, .b = 80, .a = 255};
/** @brief Button background when hovered. */
constexpr ecs::Rgba BUTTON_HOVERED = {.r = 52, .g = 72, .b = 140, .a = 255};
/** @brief Button background when pressed. */
constexpr ecs::Rgba BUTTON_PRESSED = {.r = 22, .g = 28, .b = 58, .a = 255};

/**
 * @brief Spawns an element owned by the scene, as a child of `parent`.
 * @param context Scene owning it.
 * @param parent Parent element.
 * @param anchor Placement in the parent.
 * @return The element.
 */
ecs::Entity child(scene::SceneContext& context, ecs::Entity parent, const ui::UiAnchor& anchor) {
    ecs::Registry& registry = context.registry();
    const ecs::Entity entity = context.spawn();
    registry.addComponent(entity, anchor);
    hierarchy::setParent(registry, entity, parent);
    return entity;
}

/**
 * @brief Anchor centring an element horizontally, `y` pixels from the top of its parent.
 * @param y Offset from the top.
 * @return The anchor.
 */
ui::UiAnchor centredAt(float y) { return ui::UiAnchor{.anchorX = 0.5F, .offsetY = y, .pivotX = 0.5F}; }

}  // namespace

void setScale(ecs::Registry& registry, float factor) {
    const float clamped = std::clamp(factor, MIN_SCALE, MAX_SCALE);
    if (registry.hasResource<MenuScale>()) {
        registry.getResource<MenuScale>().factor = clamped;
    } else {
        registry.emplaceResource<MenuScale>(MenuScale{.factor = clamped});
    }
}

float scale(const ecs::Registry& registry) {
    return registry.hasResource<MenuScale>() ? registry.getResource<MenuScale>().factor : 1.0F;
}

ecs::Entity panel(scene::SceneContext& context, float width, float height) {
    ecs::Registry& registry = context.registry();
    const float k = scale(registry);
    const ecs::Entity entity = context.spawn();
    registry.addComponent(entity, ui::UiAnchor{.anchorX = 0.5F, .anchorY = 0.5F, .pivotX = 0.5F, .pivotY = 0.5F});
    registry.addComponent(entity, ui::UiRect{
                                      .width = width * k,
                                      .height = height * k,
                                      .fillColor = PANEL,
                                      .borderColor = ACCENT,
                                      .borderThickness = 2.0F,
                                  });
    return entity;
}

ecs::Entity label(scene::SceneContext& context, ecs::Entity parent, std::string text, float y, float size,
                  ecs::Rgba color) {
    const float k = scale(context.registry());
    const ecs::Entity entity = child(context, parent, centredAt(y * k));
    context.registry().addComponent(entity, ui::UiText{.text = std::move(text), .fontSize = size * k, .color = color});
    return entity;
}

ecs::Entity button(scene::SceneContext& context, ecs::Entity parent, std::string text, std::string action, float y,
                   int order, float width) {
    ecs::Registry& registry = context.registry();
    const float k = scale(registry);
    const ecs::Entity entity = child(context, parent, centredAt(y * k));
    registry.addComponent(entity, ui::UiButton{
                                      .width = width * k,
                                      .height = BUTTON_HEIGHT * k,
                                      .label = std::move(text),
                                      .action = std::move(action),
                                      .colorIdle = BUTTON_IDLE,
                                      .colorHovered = BUTTON_HOVERED,
                                      .colorPressed = BUTTON_PRESSED,
                                      .colorText = TEXT,
                                      .fontSize = ui::UiButton::DEFAULT_FONT_SIZE * k,
                                  });
    registry.addComponent(entity, ui::UiFocusable{.order = order});
    return entity;
}

ecs::Entity textInput(scene::SceneContext& context, ecs::Entity parent, std::string caption, std::string text,
                      std::string action, std::string allowed, std::size_t maxLength, float y, int order) {
    (void)label(context, parent, std::move(caption), y - 26.0F, 18.0F, MUTED);
    ecs::Registry& registry = context.registry();
    const float k = scale(registry);
    const ecs::Entity entity = child(context, parent, centredAt(y * k));
    registry.addComponent(entity, ui::UiTextInput{
                                      .text = std::move(text),
                                      .action = std::move(action),
                                      .allowed = std::move(allowed),
                                      .maxLength = maxLength,
                                      .width = BUTTON_WIDTH * k,
                                      .height = ui::UiTextInput::DEFAULT_HEIGHT * k,
                                      .fontSize = ui::UiTextInput::DEFAULT_FONT_SIZE * k,
                                      .border = ACCENT,
                                  });
    registry.addComponent(entity, ui::UiFocusable{.order = order});
    return entity;
}

ecs::Entity slider(scene::SceneContext& context, ecs::Entity parent, std::string caption, float value,
                   std::string action, float y, int order) {
    ecs::Registry& registry = context.registry();
    const float k = scale(registry);
    const ecs::Entity row = child(context, parent, centredAt(y * k));
    registry.addComponent(row, ui::UiRect{.width = ROW_WIDTH * k, .height = ui::UiSlider::DEFAULT_HEIGHT * k});
    const ecs::Entity text = child(context, row, ui::UiAnchor{.anchorY = 0.5F, .pivotY = 0.5F});
    registry.addComponent(text, ui::UiText{.text = std::move(caption), .fontSize = CAPTION_SIZE * k, .color = TEXT});
    const ecs::Entity entity = child(context, row, ui::UiAnchor{.offsetX = CAPTION_WIDTH * k});
    registry.addComponent(entity, ui::UiSlider{.value = value,
                                               .step = 0.1F,
                                               .action = std::move(action),
                                               .width = (ROW_WIDTH - CAPTION_WIDTH) * k,
                                               .height = ui::UiSlider::DEFAULT_HEIGHT * k,
                                               .fill = ACCENT});
    registry.addComponent(entity, ui::UiFocusable{.order = order});
    return entity;
}

ecs::Entity toggle(scene::SceneContext& context, ecs::Entity parent, std::string caption, bool on, std::string action,
                   float y, int order) {
    ecs::Registry& registry = context.registry();
    const float k = scale(registry);
    const ecs::Entity row = child(context, parent, centredAt(y * k));
    registry.addComponent(row, ui::UiRect{.width = ROW_WIDTH * k, .height = ui::UiToggle::DEFAULT_SIZE * k});
    const ecs::Entity text = child(context, row, ui::UiAnchor{.anchorY = 0.5F, .pivotY = 0.5F});
    registry.addComponent(text, ui::UiText{.text = std::move(caption), .fontSize = CAPTION_SIZE * k, .color = TEXT});
    const ecs::Entity entity = child(context, row, ui::UiAnchor{.offsetX = CAPTION_WIDTH * k});
    registry.addComponent(entity, ui::UiToggle{.on = on,
                                               .action = std::move(action),
                                               .width = ui::UiToggle::DEFAULT_SIZE * k,
                                               .height = ui::UiToggle::DEFAULT_SIZE * k,
                                               .mark = ACCENT});
    registry.addComponent(entity, ui::UiFocusable{.order = order});
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
    } else if (registry.hasComponent<ui::UiButton>(element)) {
        std::string& current = registry.getComponents<ui::UiButton>().get(element).label;
        if (current != text) {
            current = text;
        }
    }
}

void setColor(ecs::Registry& registry, ecs::Entity element, ecs::Rgba color) {
    if (registry.hasComponent<ui::UiText>(element)) {
        registry.getComponents<ui::UiText>().get(element).color = color;
    }
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

}  // namespace client::menu
