#include "ui/system/UiRenderSystem.hpp"

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <variant>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "ecs/Registry.hpp"
#include "ui/components/FontId.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"
#include "ui/system/UiDrawItem.hpp"
#include "ui/system/UiLayout.hpp"

namespace ui {

namespace {

/** @brief Initial capacity of the per-frame queues. */
constexpr std::size_t INITIAL_QUEUE_CAPACITY = 64;

}  // namespace

UiRenderSystem::UiRenderSystem() {
    _queue.reserve(INITIAL_QUEUE_CAPACITY);
    _layout.reserve(INITIAL_QUEUE_CAPACITY);
}

UiTextMeasure UiRenderSystem::measureWith(const engine::IScreenDrawService& draw) {
    return [&draw](const UiText& text) {
        const engine::TextSize size =
            draw.measureText(text.text, text.fontSize, text.fontSize * SPACING_RATIO, text.fontId);
        return UiSize{.width = size.width, .height = size.height};
    };
}

void UiRenderSystem::collect(ecs::Registry& registry, const UiScreenRect& screen, const UiTextMeasure& measure) {
    _queue.clear();
    computeLayout(registry, screen, measure, _layout);

    // One entity may carry several drawables: they are stacked bottom to top in this fixed order.
    for (const UiLayoutNode& node : _layout) {
        const auto push = [this, &node](const auto& drawable) {
            _queue.push_back(UiDrawItem{
                .entity = node.entity,
                .screenX = node.rect.x,
                .screenY = node.rect.y,
                .drawable = drawable,
            });
        };
        if (registry.hasComponent<UiRect>(node.entity)) {
            push(registry.getComponents<UiRect>().get(node.entity));
        }
        if (registry.hasComponent<UiProgressBar>(node.entity)) {
            push(registry.getComponents<UiProgressBar>().get(node.entity));
        }
        if (registry.hasComponent<UiButton>(node.entity)) {
            push(registry.getComponents<UiButton>().get(node.entity));
        }
        if (registry.hasComponent<UiText>(node.entity)) {
            push(registry.getComponents<UiText>().get(node.entity));
        }
    }
}

void UiRenderSystem::update(ecs::Registry& registry, const UiScreenRect& screen, engine::IScreenDrawService& draw) {
    collect(registry, screen, measureWith(draw));
    submit(draw);
}

void UiRenderSystem::submit(engine::IScreenDrawService& draw) const {
    for (const UiDrawItem& item : _queue) {
        std::visit(
            [&draw, &item](const auto& drawable) {
                using T = std::decay_t<decltype(drawable)>;
                if constexpr (std::is_same_v<T, UiRect>) {
                    drawRect(draw, item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiText>) {
                    drawText(draw, item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiProgressBar>) {
                    drawProgressBar(draw, item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiButton>) {
                    drawButton(draw, item.screenX, item.screenY, drawable);
                }
            },
            item.drawable);
    }
}

void UiRenderSystem::drawRect(engine::IScreenDrawService& draw, float x, float y, const UiRect& rect) {
    const engine::ScreenRect area{.x = x, .y = y, .width = rect.width, .height = rect.height};
    if (rect.fillColor.a > 0) {
        draw.fillRect(area, rect.fillColor);
    }
    if (rect.borderThickness > 0.0F && rect.borderColor.a > 0) {
        draw.strokeRect(area, rect.borderThickness, rect.borderColor);
    }
}

void UiRenderSystem::drawProgressBar(engine::IScreenDrawService& draw, float x, float y, const UiProgressBar& bar) {
    draw.fillRect(engine::ScreenRect{.x = x, .y = y, .width = bar.width, .height = bar.height}, bar.background);

    const float range = bar.max - bar.min;
    if (range > 0.0F) {
        const float ratio = std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
        draw.fillRect(engine::ScreenRect{.x = x, .y = y, .width = bar.width * ratio, .height = bar.height},
                      bar.foreground);
    }
}

void UiRenderSystem::drawButton(engine::IScreenDrawService& draw, float x, float y, const UiButton& btn) {
    const ecs::Rgba* background = &btn.colorIdle;
    if (btn.state == UiButtonState::Pressed) {
        background = &btn.colorPressed;
    } else if (btn.state == UiButtonState::Hovered || btn.state == UiButtonState::Released) {
        background = &btn.colorHovered;
    }
    draw.fillRect(engine::ScreenRect{.x = x, .y = y, .width = btn.width, .height = btn.height}, *background);

    if (!btn.label.empty()) {
        const float spacing = btn.fontSize * SPACING_RATIO;
        const engine::TextSize label = draw.measureText(btn.label, btn.fontSize, spacing, NO_FONT);
        const float tx = x + ((btn.width - label.width) * 0.5F);
        const float ty = y + ((btn.height - label.height) * 0.5F);
        draw.drawText(btn.label, tx, ty, btn.fontSize, spacing, btn.colorText, NO_FONT);
    }
}

void UiRenderSystem::drawText(engine::IScreenDrawService& draw, float x, float y, const UiText& text) {
    draw.drawText(text.text, x, y, text.fontSize, text.fontSize * SPACING_RATIO, text.color, text.fontId);
}

}  // namespace ui
