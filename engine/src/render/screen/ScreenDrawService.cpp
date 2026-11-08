#include "render/screen/ScreenDrawService.hpp"

#include <raylib.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"

namespace render {

namespace {

/**
 * @brief Converts an engine color to a raylib color.
 * @param color Engine color.
 * @return Same color as a raylib `Color`.
 */
Color toRaylib(const ecs::Rgba& color) noexcept {
    return Color{.r = color.r, .g = color.g, .b = color.b, .a = color.a};
}

/**
 * @brief Converts an engine rectangle to a raylib rectangle.
 * @param rect Engine rectangle.
 * @return Same rectangle as a raylib `Rectangle`.
 */
Rectangle toRaylib(const engine::ScreenRect& rect) noexcept {
    return Rectangle{.x = rect.x, .y = rect.y, .width = rect.width, .height = rect.height};
}

}  // namespace

engine::FontId ScreenDrawService::loadFont(const std::string& path) { return _fonts.load(path); }

void ScreenDrawService::unloadFont(const engine::FontId font) { _fonts.unload(font); }

Font ScreenDrawService::resolveFont(engine::FontId font) const {
    const Font* stored = _fonts.get(font);
    return stored != nullptr ? *stored : GetFontDefault();
}

engine::TextSize ScreenDrawService::measureText(std::string_view text, float fontSize, float spacing,
                                                engine::FontId font) const {
    // MeasureTextEx needs a null-terminated string, which a string_view does not guarantee.
    const std::string terminated(text);
    const Vector2 size = MeasureTextEx(resolveFont(font), terminated.c_str(), fontSize, spacing);
    return {.width = size.x, .height = size.y};
}

void ScreenDrawService::fillRect(const engine::ScreenRect& rect, const ecs::Rgba& color) {
    _commands.emplace_back(FillCommand{.rect = rect, .color = color});
}

void ScreenDrawService::strokeRect(const engine::ScreenRect& rect, float thickness, const ecs::Rgba& color) {
    _commands.emplace_back(StrokeCommand{.rect = rect, .thickness = thickness, .color = color});
}

void ScreenDrawService::drawText(std::string_view text, float x, float y, float fontSize, float spacing,
                                 const ecs::Rgba& color, engine::FontId font) {
    _commands.emplace_back(TextCommand{
        .text = std::string(text),
        .x = x,
        .y = y,
        .fontSize = fontSize,
        .spacing = spacing,
        .color = color,
        .font = font,
    });
}

void ScreenDrawService::drawTexture(const ecs::TextureId texture, const ecs::TextureRect& source,
                                    const engine::ScreenRect& dest, const float rotation, const ecs::Rgba& tint) {
    _commands.emplace_back(
        TextureCommand{.texture = texture, .source = source, .dest = dest, .rotation = rotation, .tint = tint});
}

void ScreenDrawService::drawQueued(const TextureCommand& command) const {
    const Texture2D* texture = _textures != nullptr ? _textures->get(command.texture) : nullptr;
    if (texture == nullptr) {
        return;
    }
    const bool whole = command.source.width == 0 && command.source.height == 0;
    const Rectangle source = whole ? Rectangle{.x = 0.0F,
                                               .y = 0.0F,
                                               .width = static_cast<float>(texture->width),
                                               .height = static_cast<float>(texture->height),}
                                   : Rectangle{.x = static_cast<float>(command.source.x),
                                               .y = static_cast<float>(command.source.y),
                                               .width = static_cast<float>(command.source.width),
                                               .height = static_cast<float>(command.source.height),};
    // raylib rotates around `origin`, given relative to the destination's top-left corner: pivot on the center.
    const Vector2 half{.x = command.dest.width / 2.0F, .y = command.dest.height / 2.0F};
    const Rectangle dest{
        .x = command.dest.x + half.x,
        .y = command.dest.y + half.y,
        .width = command.dest.width,
        .height = command.dest.height,
    };
    DrawTexturePro(*texture, source, dest, half, command.rotation * RAD2DEG, toRaylib(command.tint));
}

void ScreenDrawService::flush() {
    for (const Command& command : _commands) {
        std::visit(
            [this](const auto& cmd) {
                using T = std::decay_t<decltype(cmd)>;
                if constexpr (std::is_same_v<T, FillCommand>) {
                    DrawRectangleRec(toRaylib(cmd.rect), toRaylib(cmd.color));
                } else if constexpr (std::is_same_v<T, StrokeCommand>) {
                    DrawRectangleLinesEx(toRaylib(cmd.rect), cmd.thickness, toRaylib(cmd.color));
                } else if constexpr (std::is_same_v<T, TextCommand>) {
                    DrawTextEx(resolveFont(cmd.font), cmd.text.c_str(), Vector2{.x = cmd.x, .y = cmd.y}, cmd.fontSize,
                               cmd.spacing, toRaylib(cmd.color));
                } else if constexpr (std::is_same_v<T, TextureCommand>) {
                    drawQueued(cmd);
                }
            },
            command);
    }
    _commands.clear();
}

}  // namespace render
