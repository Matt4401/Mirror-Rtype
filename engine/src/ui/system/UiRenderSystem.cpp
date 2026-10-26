#include "ui/system/UiRenderSystem.hpp"

#include <raylib.h>

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
}

}  // namespace ui
