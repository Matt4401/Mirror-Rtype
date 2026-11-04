#include "ui/system/UiRenderSystem.hpp"

#include <raylib.h>

#include <algorithm>
#include <type_traits>
#include <variant>

#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiButton.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace ui {

static constexpr std::size_t INITIAL_QUEUE_CAPACITY = 64;

UiRenderSystem::UiRenderSystem(const FontStore& fonts) : _fonts(&fonts) { _queue.reserve(INITIAL_QUEUE_CAPACITY); }

void UiRenderSystem::collect(ecs::Registry& registry) {
    _queue.clear();

    const auto screenW = static_cast<float>(GetScreenWidth());
    const auto screenH = static_cast<float>(GetScreenHeight());

    const auto resolve = [screenW, screenH](const UiAnchor& anchor, float elemW,
                                            float elemH) -> std::pair<float, float> {
        return {(anchor.anchorX * screenW) + anchor.offsetX - (anchor.pivotX * elemW),
                (anchor.anchorY * screenH) + anchor.offsetY - (anchor.pivotY * elemH)};
    };

    registry.runSystem<UiAnchor, UiRect>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiRect& rect) {
            auto [x, y] = resolve(anchor, rect.width, rect.height);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = rect});
        });

    registry.runSystem<UiAnchor, UiText>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiText& text) {
            const Font* stored = _fonts->get(text.fontId);
            const Font font = (stored != nullptr) ? *stored : GetFontDefault();
            const float spacing = text.fontSize / 10.0F;
            const Vector2 measured = MeasureTextEx(font, text.text.c_str(), text.fontSize, spacing);
            auto [x, y] = resolve(anchor, measured.x, measured.y);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = text});
        });

    registry.runSystem<UiAnchor, UiProgressBar>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiProgressBar& bar) {
            auto [x, y] = resolve(anchor, bar.width, bar.height);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = bar});
        });

    registry.runSystem<UiAnchor, UiButton>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiButton& btn) {
            auto [x, y] = resolve(anchor, btn.width, btn.height);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = btn});
        });
}

void UiRenderSystem::update(ecs::Registry& registry) {
    collect(registry);
    draw();
}

void UiRenderSystem::draw() const {
    for (const UiDrawItem& item : _queue) {
        std::visit(
            [this, &item](const auto& drawable) {
                using T = std::decay_t<decltype(drawable)>;
                if constexpr (std::is_same_v<T, UiRect>) {
                    drawRect(item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiText>) {
                    drawText(item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiProgressBar>) {
                    drawProgressBar(item.screenX, item.screenY, drawable);
                } else if constexpr (std::is_same_v<T, UiButton>) {
                    drawButton(item.screenX, item.screenY, drawable);
                }
            },
            item.drawable);
    }
}

void UiRenderSystem::drawRect(float x, float y, const UiRect& rect) {
    if (rect.fillColor.a > 0) {
        const Color fill{.r = rect.fillColor.r, .g = rect.fillColor.g, .b = rect.fillColor.b, .a = rect.fillColor.a};
        DrawRectangleV({.x = x, .y = y}, {.x = rect.width, .y = rect.height}, fill);
    }
    if (rect.borderThickness > 0.0F && rect.borderColor.a > 0) {
        const Color border{
            .r = rect.borderColor.r,
            .g = rect.borderColor.g,
            .b = rect.borderColor.b,
            .a = rect.borderColor.a,
        };
        DrawRectangleLinesEx({.x = x, .y = y, .width = rect.width, .height = rect.height}, rect.borderThickness,
                             border);
    }
}

void UiRenderSystem::drawProgressBar(float x, float y, const UiProgressBar& bar) {
    const Color bg{.r = bar.background.r, .g = bar.background.g, .b = bar.background.b, .a = bar.background.a};
    DrawRectangleV({.x = x, .y = y}, {.x = bar.width, .y = bar.height}, bg);

    const float range = bar.max - bar.min;
    if (range > 0.0F) {
        const float ratio = std::clamp((bar.value - bar.min) / range, 0.0F, 1.0F);
        const Color fg{.r = bar.foreground.r, .g = bar.foreground.g, .b = bar.foreground.b, .a = bar.foreground.a};
        DrawRectangleV({.x = x, .y = y}, {.x = bar.width * ratio, .y = bar.height}, fg);
    }
}

void UiRenderSystem::drawButton(float x, float y, const UiButton& btn) const {
    const ecs::Rgba* bgPtr = &btn.colorIdle;
    if (btn.state == UiButtonState::Pressed) {
        bgPtr = &btn.colorPressed;
    } else if (btn.state == UiButtonState::Hovered) {
        bgPtr = &btn.colorHovered;
    }
    const ecs::Rgba& bg = *bgPtr;
    DrawRectangleV({.x = x, .y = y}, {.x = btn.width, .y = btn.height},
                   Color{.r = bg.r, .g = bg.g, .b = bg.b, .a = bg.a});

    if (!btn.label.empty()) {
        const Font* stored = _fonts->get(ui::NO_FONT);
        const Font font = (stored != nullptr) ? *stored : GetFontDefault();
        const float spacing = btn.fontSize / 10.0F;
        const Vector2 measured = MeasureTextEx(font, btn.label.c_str(), btn.fontSize, spacing);
        const float tx = x + ((btn.width - measured.x) * 0.5F);
        const float ty = y + ((btn.height - measured.y) * 0.5F);
        const Color tc{.r = btn.colorText.r, .g = btn.colorText.g, .b = btn.colorText.b, .a = btn.colorText.a};
        DrawTextEx(font, btn.label.c_str(), {.x = tx, .y = ty}, btn.fontSize, spacing, tc);
    }
}

void UiRenderSystem::drawText(float x, float y, const UiText& text) const {
    const Font* stored = _fonts->get(text.fontId);
    const Font font = (stored != nullptr) ? *stored : GetFontDefault();
    const Color color{.r = text.color.r, .g = text.color.g, .b = text.color.b, .a = text.color.a};
    const float spacing = text.fontSize / 10.0F;
    DrawTextEx(font, text.text.c_str(), {.x = x, .y = y}, text.fontSize, spacing, color);
}

}  // namespace ui
