#include "ui/system/UiRenderSystem.hpp"

#include <algorithm>
#include <cstddef>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ui/components/FontId.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiNavigation.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiSlider.hpp"
#include "ui/components/UiText.hpp"
#include "ui/components/UiTextInput.hpp"
#include "ui/components/UiToggle.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

namespace {

/** @brief Capacity reserved for the layout, so the first frames do not grow it step by step. */
constexpr std::size_t INITIAL_LAYOUT_CAPACITY = 64;

/** @brief Inset of a toggle's mark inside its box, as a fraction of the box size. */
constexpr float TOGGLE_MARK_INSET = 0.22F;

/** @brief Width of the text input caret in pixels. */
constexpr float CARET_WIDTH = 2.0F;

/**
 * @brief Draws the component `C` of a node, if it has one.
 * @tparam C Component type.
 * @tparam Draw Drawing function.
 * @param registry Registry holding the node.
 * @param node Laid out node.
 * @param draw Drawing function called with the component.
 */
template <typename C, typename Draw>
void drawIf(const ecs::Registry& registry, const UiLayoutNode& node, const Draw& draw) {
    const auto* components = registry.findComponents<C>();
    if (components != nullptr && components->contains(node.entity)) {
        draw(components->get(node.entity));
    }
}

}  // namespace

UiRenderSystem::UiRenderSystem() { _layout.reserve(INITIAL_LAYOUT_CAPACITY); }

UiTextMeasure UiRenderSystem::measureWith(const engine::IScreenDrawService& draw) {
    return [&draw](const UiText& text) {
        const engine::TextSize size =
            draw.measureText(text.text, text.fontSize, text.fontSize * SPACING_RATIO, text.fontId);
        return UiSize{.width = size.width, .height = size.height};
    };
}

void UiRenderSystem::collect(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure) {
    computeLayout(registry, screen, measure, _layout);
}

void UiRenderSystem::update(ecs::Registry& registry, const UiScreenRect& screen, engine::IScreenDrawService& draw) {
    collect(registry, screen, measureWith(draw));
    submit(registry, draw);
}

void UiRenderSystem::submit(const ecs::Registry& registry, engine::IScreenDrawService& draw) const {
    const UiFocus* focus = registry.hasResource<UiFocus>() ? &registry.getResource<UiFocus>() : nullptr;
    const UiNavigation* navigation =
        registry.hasResource<UiNavigation>() ? &registry.getResource<UiNavigation>() : nullptr;
    const ecs::Entity focused = focus != nullptr ? focus->focused : ecs::NULL_ENTITY;

    for (const UiLayoutNode& node : _layout) {
        const UiScreenRect& area = node.rect;
        drawIf<UiRect>(registry, node, [&](const UiRect& rect) { drawRect(draw, area, rect); });
        drawIf<UiProgressBar>(registry, node, [&](const UiProgressBar& bar) { drawProgressBar(draw, area, bar); });
        drawIf<UiButton>(registry, node, [&](const UiButton& btn) { drawButton(draw, area, btn); });
        drawIf<UiToggle>(registry, node, [&](const UiToggle& toggle) { drawToggle(draw, area, toggle); });
        drawIf<UiSlider>(registry, node, [&](const UiSlider& slider) { drawSlider(draw, area, slider); });
        drawIf<UiTextInput>(registry, node, [&](const UiTextInput& input) {
            drawTextInput(draw, area, input, node.entity == focused && focus->caretVisible);
        });
        drawIf<UiText>(registry, node, [&](const UiText& text) { drawText(draw, area, text); });

        if (node.entity == focused && navigation != nullptr && navigation->focusThickness > 0.0F) {
            draw.strokeRect(UiScreenRect{.x = area.x - FOCUS_GAP,
                                         .y = area.y - FOCUS_GAP,
                                         .width = area.width + (2.0F * FOCUS_GAP),
                                         .height = area.height + (2.0F * FOCUS_GAP)},
                            navigation->focusThickness, navigation->focusColor);
        }
    }
}

void UiRenderSystem::drawRect(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiRect& rect) {
    const engine::ScreenRect box{.x = area.x, .y = area.y, .width = rect.width, .height = rect.height};
    if (rect.fillColor.a > 0) {
        draw.fillRect(box, rect.fillColor);
    }
    if (rect.borderThickness > 0.0F && rect.borderColor.a > 0) {
        draw.strokeRect(box, rect.borderThickness, rect.borderColor);
    }
}

void UiRenderSystem::drawProgressBar(engine::IScreenDrawService& draw, const UiScreenRect& area,
                                     const UiProgressBar& bar) {
    draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = bar.width, .height = bar.height},
                  bar.background);

    const float range = bar.max - bar.min;
    if (range > 0.0F) {
        const float ratio = std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
        draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = bar.width * ratio, .height = bar.height},
                      bar.foreground);
    }
}

void UiRenderSystem::drawButton(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiButton& btn) {
    const ecs::Rgba* background = &btn.colorIdle;
    if (btn.state == UiButtonState::Pressed) {
        background = &btn.colorPressed;
    } else if (btn.state == UiButtonState::Hovered || btn.state == UiButtonState::Released) {
        background = &btn.colorHovered;
    }
    draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = btn.width, .height = btn.height}, *background);

    if (!btn.label.empty()) {
        const float spacing = btn.fontSize * SPACING_RATIO;
        const engine::TextSize label = draw.measureText(btn.label, btn.fontSize, spacing, NO_FONT);
        const float tx = area.x + ((btn.width - label.width) * 0.5F);
        const float ty = area.y + ((btn.height - label.height) * 0.5F);
        draw.drawText(btn.label, tx, ty, btn.fontSize, spacing, btn.colorText, NO_FONT);
    }
}

void UiRenderSystem::drawToggle(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiToggle& toggle) {
    draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = toggle.width, .height = toggle.height},
                  toggle.box);
    if (toggle.on) {
        const float insetX = toggle.width * TOGGLE_MARK_INSET;
        const float insetY = toggle.height * TOGGLE_MARK_INSET;
        draw.fillRect(engine::ScreenRect{.x = area.x + insetX,
                                         .y = area.y + insetY,
                                         .width = toggle.width - (2.0F * insetX),
                                         .height = toggle.height - (2.0F * insetY)},
                      toggle.mark);
    }
}

void UiRenderSystem::drawSlider(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiSlider& slider) {
    draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = slider.width, .height = slider.height},
                  slider.track);
    const float range = slider.max - slider.min;
    const float ratio = range > 0.0F ? std::clamp((slider.value - slider.min) / range, 0.0F, 1.0F) : 0.0F;
    const float filled = slider.width * ratio;
    draw.fillRect(engine::ScreenRect{.x = area.x, .y = area.y, .width = filled, .height = slider.height}, slider.fill);
    const float knob = slider.height + 4.0F;
    draw.fillRect(
        engine::ScreenRect{.x = area.x + filled - (knob * 0.5F), .y = area.y - 2.0F, .width = knob, .height = knob},
        slider.knob);
}

void UiRenderSystem::drawTextInput(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiTextInput& input,
                                   bool caret) {
    const engine::ScreenRect box{.x = area.x, .y = area.y, .width = input.width, .height = input.height};
    draw.fillRect(box, input.background);
    draw.strokeRect(box, 1.0F, input.border);

    const float spacing = input.fontSize * SPACING_RATIO;
    const bool empty = input.text.empty();
    const std::string& shown = empty ? input.placeholder : input.text;
    const engine::TextSize size = draw.measureText(shown, input.fontSize, spacing, input.fontId);
    const float tx = area.x + TEXT_INPUT_PADDING;
    const float ty = area.y + ((input.height - input.fontSize) * 0.5F);
    if (!shown.empty()) {
        draw.drawText(shown, tx, ty, input.fontSize, spacing, empty ? input.placeholderColor : input.textColor,
                      input.fontId);
    }
    if (caret) {
        const float cx = tx + (empty ? 0.0F : size.width) + 1.0F;
        draw.fillRect(engine::ScreenRect{.x = cx, .y = ty, .width = CARET_WIDTH, .height = input.fontSize},
                      input.textColor);
    }
}

void UiRenderSystem::drawText(engine::IScreenDrawService& draw, const UiScreenRect& area, const UiText& text) {
    draw.drawText(text.text, area.x, area.y, text.fontSize, text.fontSize * SPACING_RATIO, text.color, text.fontId);
}

}  // namespace ui
