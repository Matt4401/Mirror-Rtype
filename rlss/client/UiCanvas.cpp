#include "UiCanvas.hpp"

#include <string_view>

#include "PixelFont.hpp"
#include "Shapes.hpp"
#include "components/CameraComponent.hpp"
#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"

namespace rlss::client {

ecs::Entity UiCanvas::rect(ecs::Registry& registry, const ScreenRect& rect, const ecs::Rgba color, const int layer) {
    const ecs::Entity entity = registry.spawnEntity();
    // Invisible until `place()` puts it on screen.
    registry.addComponent(entity, ecs::Transform{.x = 0.0F, .y = 0.0F, .scaleX = 0.0F, .scaleY = 0.0F});
    registry.addComponent(entity, makeRenderable(color, layer));
    _items.push_back({.entity = entity, .rect = rect});
    return entity;
}

void UiCanvas::text(ecs::Registry& registry, const std::string_view text, const float x, const float top,
                    const float square, const ecs::Rgba color, const int layer, const Align align) {
    float left = x;
    if (align == Align::Center) {
        left -= textWidth(text, square) / 2.0F;
    } else if (align == Align::Right) {
        left -= textWidth(text, square);
    }
    font::forEachRun(text, [&](const font::Run& run) {
        rect(registry,
             ScreenRect{
                 .x = left + (static_cast<float>(run.column) * square),
                 .y = top + (static_cast<float>(run.row) * square),
                 .width = static_cast<float>(run.length) * square,
                 .height = square,
             },
             color, layer);
    });
}

float UiCanvas::textWidth(const std::string_view text, const float square) noexcept {
    return static_cast<float>(font::width(text)) * square;
}

void UiCanvas::clear(ecs::Registry& registry) {
    for (const Item& item : _items) {
        if (registry.isAlive(item.entity)) {
            registry.killEntity(item.entity);
        }
    }
    _items.clear();
}

void UiCanvas::place(ecs::Registry& registry, const ecs::CameraComponent& camera, const float shiftX,
                     const float shiftY) const {
    auto& transforms = registry.getComponents<ecs::Transform>();
    const float zoom = camera.zoom > 0.0F ? camera.zoom : 1.0F;
    for (const Item& item : _items) {
        if (!transforms.contains(item.entity)) {
            continue;
        }
        // Screen to world: world = target + (screen - offset) / zoom. A Renderable is drawn around its center.
        ecs::Transform& transform = transforms.get(item.entity);
        transform.x = camera.targetX + ((shiftX + item.rect.x + (item.rect.width / 2.0F) - camera.offsetX) / zoom);
        transform.y = camera.targetY + ((shiftY + item.rect.y + (item.rect.height / 2.0F) - camera.offsetY) / zoom);
        transform.rotation = 0.0F;
        transform.scaleX = item.rect.width / (PLACEHOLDER_SIZE * zoom);
        transform.scaleY = item.rect.height / (PLACEHOLDER_SIZE * zoom);
    }
}

}  // namespace rlss::client
