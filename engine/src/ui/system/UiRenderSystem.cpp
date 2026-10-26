#include "ui/system/UiRenderSystem.hpp"

#include <raylib.h>

#include <type_traits>
#include <variant>

#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiProgressBar.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiText.hpp"

namespace ui {

static constexpr std::size_t INITIAL_QUEUE_CAPACITY = 64;

UiRenderSystem::UiRenderSystem(const FontStore& fonts) : _fonts(&fonts) {
    _queue.reserve(INITIAL_QUEUE_CAPACITY);
}

void UiRenderSystem::collect(ecs::Registry& registry) {
    _queue.clear();

    const auto screenW = static_cast<float>(GetScreenWidth());
    const auto screenH = static_cast<float>(GetScreenHeight());

    const auto resolve = [screenW, screenH](const UiAnchor& anchor) -> std::pair<float, float> {
        return {(anchor.anchorX * screenW) + anchor.offsetX, (anchor.anchorY * screenH) + anchor.offsetY};
    };

    registry.runSystem<UiAnchor, UiRect>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiRect& rect) {
            auto [x, y] = resolve(anchor);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = rect});
        });

    registry.runSystem<UiAnchor, UiText>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiText& text) {
            auto [x, y] = resolve(anchor);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = text});
        });

    registry.runSystem<UiAnchor, UiProgressBar>(
        [this, &resolve](ecs::Entity entity, const UiAnchor& anchor, const UiProgressBar& bar) {
            auto [x, y] = resolve(anchor);
            _queue.push_back(UiDrawItem{.entity = entity, .screenX = x, .screenY = y, .drawable = bar});
        });
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
            .r = rect.borderColor.r, .g = rect.borderColor.g, .b = rect.borderColor.b, .a = rect.borderColor.a,};
        DrawRectangleLinesEx({.x = x, .y = y, .width = rect.width, .height = rect.height}, rect.borderThickness, border);
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
