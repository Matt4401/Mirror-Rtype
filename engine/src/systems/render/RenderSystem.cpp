#include "systems/render/RenderSystem.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "render/CameraComponent.hpp"
#include "systems/render/RenderSystemDefaults.hpp"
#include "systems/render/RenderSystemSort.hpp"
#include "render/texture/TextureStore.hpp"

namespace render {

RenderSystem::RenderSystem(const TextureStore& textures) : _textures(&textures), _camera(detail::neutralCamera()) {
    _queue.reserve(detail::INITIAL_QUEUE_CAPACITY);
}

void RenderSystem::collect(ecs::Registry& registry) {
    _queue.clear();
    registry.runSystem<ecs::Transform, ecs::Renderable>(
        [this](ecs::Entity entity, const ecs::Transform& transform, const ecs::Renderable& renderable) {
            _queue.push_back(DrawItem{
                .entity = entity,
                .layer = renderable.layer,
                .textureId = renderable.textureId,
                .sourceRect = renderable.sourceRect,
                .origin = renderable.origin,
                .transform = transform,
                .tint = renderable.tint,
            });
        });
    std::ranges::sort(_queue, detail::drawsBefore);

    _camera = detail::neutralCamera();
    bool cameraFound = false;
    registry.runSystem<CameraComponent>([this, &cameraFound](ecs::Entity /*entity*/, const CameraComponent& component) {
        if (!cameraFound && component.active) {
            _camera = component.camera;
            cameraFound = true;
        }
    });
}

void RenderSystem::draw() const {
    BeginMode2D(_camera);
    for (const DrawItem& item : _queue) {
        drawItem(item);
    }
    EndMode2D();
}

void RenderSystem::update(ecs::Registry& registry) {
    collect(registry);
    draw();
}

void RenderSystem::drawItem(const DrawItem& item) const {
    const ecs::Transform& transform = item.transform;
    const Color tint{.r = item.tint.r, .g = item.tint.g, .b = item.tint.b, .a = item.tint.a};
    const float degrees = transform.rotation * detail::RADIANS_TO_DEGREES;

    if (item.textureId == ecs::NO_TEXTURE) {
        const float width = PLACEHOLDER_SIZE * std::fabs(transform.scaleX);
        const float height = PLACEHOLDER_SIZE * std::fabs(transform.scaleY);
        const Rectangle rect{.x = transform.x, .y = transform.y, .width = width, .height = height};
        DrawRectanglePro(rect, Vector2{.x = width * item.origin.x, .y = height * item.origin.y}, degrees, tint);
        return;
    }

    const Texture2D* texture = _textures->get(item.textureId);
    if (texture == nullptr) {
        return;
    }

    const auto textureWidth = static_cast<float>(texture->width);
    const auto textureHeight = static_cast<float>(texture->height);
    const bool usesWholeTexture = item.sourceRect.width == 0 && item.sourceRect.height == 0;
    const float sourceX = usesWholeTexture ? 0.0F : static_cast<float>(item.sourceRect.x);
    const float sourceY = usesWholeTexture ? 0.0F : static_cast<float>(item.sourceRect.y);
    const float sourceWidth = usesWholeTexture ? textureWidth : static_cast<float>(item.sourceRect.width);
    const float sourceHeight = usesWholeTexture ? textureHeight : static_cast<float>(item.sourceRect.height);
    // A negative source size mirrors the texture, which is how negative scales flip a sprite.
    const Rectangle source{
        .x = sourceX,
        .y = sourceY,
        .width = transform.scaleX < 0.0F ? -sourceWidth : sourceWidth,
        .height = transform.scaleY < 0.0F ? -sourceHeight : sourceHeight,
    };
    const float destWidth = sourceWidth * std::fabs(transform.scaleX);
    const float destHeight = sourceHeight * std::fabs(transform.scaleY);
    const Rectangle dest{.x = transform.x, .y = transform.y, .width = destWidth, .height = destHeight};
    DrawTexturePro(*texture, source, dest, Vector2{.x = destWidth * item.origin.x, .y = destHeight * item.origin.y},
                   degrees, tint);
}

}  // namespace render
