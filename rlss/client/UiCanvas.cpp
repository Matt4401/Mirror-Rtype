#include "UiCanvas.hpp"

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"

namespace rlss::client {

namespace {

// Metrics of Russo One (rlss/assets/fonts), as the render backend sizes fonts: by ascender + descender.
/** @brief Height of the capital letters, as a share of the font size. */
constexpr float CAP_RATIO = 700.0F / 1205.0F;
/** @brief Gap between the top of a drawn text and the top of its capitals, as a share of the font size. */
constexpr float CAP_TOP_RATIO = 226.0F / 1205.0F;
/** @brief Extra space between the characters, as a share of the font size. */
constexpr float SPACING_RATIO = 0.02F;

}  // namespace

float Typeface::width(const std::string_view text, const float capHeight) const {
    if (_draw == nullptr || text.empty()) {
        return 0.0F;
    }
    const float size = capHeight / CAP_RATIO;
    return _draw->measureText(text, size, size * SPACING_RATIO, _font).width;
}

void Typeface::draw(const std::string_view text, const float x, const float capTop, const float capHeight,
                    const ecs::Rgba color, const Align align) const {
    if (_draw == nullptr || text.empty()) {
        return;
    }
    const float size = capHeight / CAP_RATIO;
    float left = x;
    if (align != Align::Left) {
        const float textWidth = width(text, capHeight);
        left -= align == Align::Center ? textWidth / 2.0F : textWidth;
    }
    _draw->drawText(text, left, capTop - (size * CAP_TOP_RATIO), size, size * SPACING_RATIO, color, _font);
}

UiHandle UiCanvas::rect(const ScreenRect& rect, const ecs::Rgba color) {
    _items.push_back({.shape = Fill{.rect = rect}, .color = color});
    return _items.size() - 1;
}

UiHandle UiCanvas::outline(const ScreenRect& rect, const float thickness, const ecs::Rgba color) {
    _items.push_back({.shape = Outline{.rect = rect, .thickness = thickness}, .color = color});
    return _items.size() - 1;
}

UiHandle UiCanvas::image(const ecs::TextureId texture, const ScreenRect& rect, const ecs::Rgba tint) {
    _items.push_back({.shape = Image{.texture = texture, .rect = rect}, .color = tint});
    return _items.size() - 1;
}

UiHandle UiCanvas::text(const std::string_view text, const float x, const float capTop, const float capHeight,
                        const ecs::Rgba color, const Align align) {
    _items.push_back({
        .shape = Text{.text = std::string(text), .x = x, .capTop = capTop, .capHeight = capHeight, .align = align},
        .color = color,
    });
    return _items.size() - 1;
}

void UiCanvas::setColor(const UiHandle handle, const ecs::Rgba color) {
    if (handle < _items.size()) {
        _items.at(handle).color = color;
    }
}

void UiCanvas::draw(const Typeface& typeface, const float shiftX, const float shiftY) const {
    engine::IScreenDrawService* service = typeface.service();
    if (service == nullptr) {
        return;
    }
    const auto moved = [shiftX, shiftY](const ScreenRect& rect) {
        return engine::ScreenRect{
            .x = rect.x + shiftX,
            .y = rect.y + shiftY,
            .width = rect.width,
            .height = rect.height,
        };
    };
    for (const Item& item : _items) {
        if (item.color.a == 0) {
            continue;
        }
        std::visit(
            [&](const auto& shape) {
                using T = std::decay_t<decltype(shape)>;
                if constexpr (std::is_same_v<T, Fill>) {
                    service->fillRect(moved(shape.rect), item.color);
                } else if constexpr (std::is_same_v<T, Outline>) {
                    service->strokeRect(moved(shape.rect), shape.thickness, item.color);
                } else if constexpr (std::is_same_v<T, Image>) {
                    if (shape.texture != ecs::NO_TEXTURE) {
                        service->drawTexture(shape.texture, {}, moved(shape.rect), 0.0F, item.color);
                    }
                } else {
                    typeface.draw(shape.text, shape.x + shiftX, shape.capTop + shiftY, shape.capHeight, item.color,
                                  shape.align);
                }
            },
            item.shape);
    }
}

}  // namespace rlss::client
