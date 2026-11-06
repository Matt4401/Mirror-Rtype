/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side Bydos enemy prefab: animated sprite flying left, hittable by player shots.
*/

#include "prefabs/ClientEnemy.hpp"

#include <cstdint>

#include "components/AnimationComponent.hpp"
#include "components/Collider2D.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/CollisionCategories.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

namespace {

constexpr int FRAME_WIDTH = 33;
constexpr int FRAME_HEIGHT = 36;
/** @brief Collider half extents before the transform scale, a bit smaller than the frame to ignore its margins. */
constexpr float HALF_WIDTH = 14.0F;
constexpr float HALF_HEIGHT = 15.0F;

}  // namespace

void buildClientEnemy(ecs::Entity e, ecs::Registry& registry, ::prefabs::PrefabParams const& params) {
    const auto texId = static_cast<ecs::TextureId>(params.get<std::int64_t>("textureId", 0));
    const float speed = params.get("speed", DEFAULT_CLIENT_ENEMY_SPEED);

    registry.addComponent<ecs::Transform>(e, ::prefabs::makeTransform(params));
    registry.addComponent<ecs::Renderable>(
        e, ecs::Renderable{
               .textureId = texId,
               .sourceRect = {.x = 0, .y = 0, .width = FRAME_WIDTH, .height = FRAME_HEIGHT},
               .origin = {.x = 0.5F, .y = 0.5F},
               .tint = {.r = 255, .g = 255, .b = 255, .a = 255},
               .layer = 2,
           });
    registry.addComponent<ecs::AnimationComponent>(e, ecs::AnimationComponent{
                                                          .frameWidth = FRAME_WIDTH,
                                                          .frameHeight = FRAME_HEIGHT,
                                                          .frameCount = 8,
                                                          .startX = 0,
                                                          .startY = 0,
                                                          .frameDuration = 0.12F,
                                                      });
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Kinematic,
                                                   .velocityX = -speed,
                                                   .fixedRotation = true,
                                               });
    registry.addComponent<ecs::Collider2D>(e, ecs::Collider2D{
                                                  .halfWidth = HALF_WIDTH,
                                                  .halfHeight = HALF_HEIGHT,
                                                  .categoryBits = ::prefabs::collision::ENEMY,
                                                  .maskBits = ::prefabs::collision::ENEMY_MASK,
                                                  .isSensor = true,
                                              });
    registry.addComponent<ClientEnemy>(e, ClientEnemy{});
}

}  // namespace client::prefabs
