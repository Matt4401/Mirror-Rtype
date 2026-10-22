/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client-side player bullet prefab: animated sprite driven by the physics system.
*/

#include "prefabs/ClientBullet.hpp"

#include "components/AnimationComponent.hpp"
#include "components/Renderable.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

void buildClientBullet(ecs::Entity e, ecs::Registry& registry, ::prefabs::PrefabParams const& params) {
    const auto texId = static_cast<ecs::TextureId>(params.get<std::int64_t>("textureId", 0));
    const float speed = params.get("speed", DEFAULT_CLIENT_BULLET_SPEED);

    registry.addComponent<ecs::Transform>(e, ::prefabs::makeTransform(params));
    registry.addComponent<ecs::Renderable>(e, ecs::Renderable{
                                                  .textureId = texId,
                                                  .sourceRect = {.x = 0, .y = 0, .width = 32, .height = 32},
                                                  .origin = {.x = 0.5F, .y = 0.5F},
                                                  .tint = {.r = 255, .g = 255, .b = 255, .a = 255},
                                                  .layer = 2,
                                              });
    registry.addComponent<ecs::AnimationComponent>(e, ecs::AnimationComponent{
                                                          .frameWidth = 32,
                                                          .frameHeight = 32,
                                                          .frameCount = 4,
                                                          .startX = 0,
                                                          .startY = 0,
                                                          .frameDuration = 0.015F,
                                                          .loop = true,
                                                      });
    registry.addComponent<ecs::RigidBody2D>(e, ecs::RigidBody2D{
                                                   .bodyType = ecs::BodyType::Dynamic,
                                                   .velocityX = speed,
                                                   .fixedRotation = true,
                                               });
    registry.addComponent<ClientBullet>(e, ClientBullet{});
}

}  // namespace client::prefabs
