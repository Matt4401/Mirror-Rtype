#include "render/system/RenderSystem.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>

#include "components/Renderable.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "render/CameraComponent.hpp"
#include "render/system/RenderSystemDefaults.hpp"
#include "render/texture/TextureStore.hpp"

namespace render {

namespace {

bool drawsBefore(const DrawItem& lhs, const DrawItem& rhs) {
    if (lhs.layer != rhs.layer) {
        return lhs.layer < rhs.layer;
    }
    if (lhs.transform.z != rhs.transform.z) {
        return lhs.transform.z < rhs.transform.z;
    }
    return lhs.entity < rhs.entity;
}

}  // namespace

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
                .transform = transform,
                .tint = renderable.tint,
            });
        });
    std::ranges::sort(_queue, drawsBefore);

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
        DrawRectanglePro(rect, Vector2{.x = width / 2.0F, .y = height / 2.0F}, degrees, tint);
        return;
    }

    const Texture2D* texture = _textures->get(item.textureId);
    if (texture == nullptr) {
        return;
    }

    const auto textureWidth = static_cast<float>(texture->width);
    const auto textureHeight = static_cast<float>(texture->height);
    // A negative source size mirrors the texture, which is how negative scales flip a sprite.
    const Rectangle source{
        .x = 0.0F,
        .y = 0.0F,
        .width = transform.scaleX < 0.0F ? -textureWidth : textureWidth,
        .height = transform.scaleY < 0.0F ? -textureHeight : textureHeight,
    };
    const float destWidth = textureWidth * std::fabs(transform.scaleX);
    const float destHeight = textureHeight * std::fabs(transform.scaleY);
    const Rectangle dest{.x = transform.x, .y = transform.y, .width = destWidth, .height = destHeight};
    DrawTexturePro(*texture, source, dest, Vector2{.x = destWidth / 2.0F, .y = destHeight / 2.0F}, degrees, tint);
}

}  // namespace render
