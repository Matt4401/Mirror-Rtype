#include "systems/physics/PhysicsSystem.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>

#include <algorithm>
#include <cmath>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "systems/physics/PhysicsConstants.hpp"
#include "systems/physics/PhysicsHelpers.hpp"

namespace systems {

PhysicsSystem::PhysicsSystem(ecs::Registry& registry)
    : _registry(&registry),
      _world(b2Vec2_zero),
      _destroyListenerId(_registry->addDestroyListener([this](ecs::Entity entity) { destroyBody(entity); })),
      _rigidBodyRemoveListenerId(_registry->addComponentRemoveListener(
          [this](ecs::Entity entity, ecs::ComponentTypeId componentType) {
              if (componentType == ecs::getComponentTypeId<ecs::RigidBody2D>()) {
                  destroyBody(entity);
              }
          })) {}

PhysicsSystem::~PhysicsSystem() {
    _registry->removeDestroyListener(_destroyListenerId);
    _registry->removeComponentRemoveListener(_rigidBodyRemoveListenerId);
}

void PhysicsSystem::update(float fixedDt) {
    if (fixedDt <= 0.0F) {
        return;
    }

    _registry->runSystem<ecs::Transform, ecs::RigidBody2D>(
        [this](ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
            synchronizeBody(entity, transform, rigidBody);
        });

    _world.Step(fixedDt, physics::VELOCITY_ITERATIONS, physics::POSITION_ITERATIONS);

    _registry->runSystem<ecs::Transform, ecs::RigidBody2D>(
        [this](ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
            if (rigidBody.bodyType == ecs::BodyType::Static) {
                return;
            }

            const auto it = _bodies.find(entity);
            if (it == _bodies.end()) {
                return;
            }

            const b2Vec2 position = it->second->GetPosition();
            const b2Vec2 velocity = it->second->GetLinearVelocity();
            transform.x = position.x * physics::PPM;
            transform.y = position.y * physics::PPM;
            transform.rotation = it->second->GetAngle();
            rigidBody.velocityX = velocity.x * physics::PPM;
            rigidBody.velocityY = velocity.y * physics::PPM;
        });
}

b2Body* PhysicsSystem::createBody(ecs::Entity entity, const ecs::Transform& transform,
                                  const ecs::RigidBody2D& rigidBody) {
    b2BodyDef bodyDef;
    bodyDef.type = physics::toBox2DBodyType(rigidBody.bodyType);
    bodyDef.position.Set(transform.x / physics::PPM, transform.y / physics::PPM);
    bodyDef.angle = transform.rotation;
    bodyDef.linearVelocity.Set(rigidBody.velocityX / physics::PPM, rigidBody.velocityY / physics::PPM);
    bodyDef.linearDamping = rigidBody.linearDamping;
    bodyDef.angularDamping = rigidBody.angularDamping;
    bodyDef.fixedRotation = rigidBody.fixedRotation;
    bodyDef.bullet = rigidBody.bullet;

    b2Body* body = _world.CreateBody(&bodyDef);
    _bodies.emplace(entity, body);
    synchronizeCollider(entity, transform, *body);
    return body;
}

void PhysicsSystem::synchronizeBody(ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
    auto it = _bodies.find(entity);
    if (it == _bodies.end()) {
        createBody(entity, transform, rigidBody);
        return;
    }

    b2Body& body = *it->second;
    body.SetType(physics::toBox2DBodyType(rigidBody.bodyType));
    body.SetLinearDamping(rigidBody.linearDamping);
    body.SetAngularDamping(rigidBody.angularDamping);
    body.SetFixedRotation(rigidBody.fixedRotation);
    body.SetBullet(rigidBody.bullet);

    const b2Vec2 currentPosition = body.GetPosition();
    const float targetX = transform.x / physics::PPM;
    const float targetY = transform.y / physics::PPM;
    if (std::fabs(currentPosition.x - targetX) > physics::POSITION_EPSILON ||
        std::fabs(currentPosition.y - targetY) > physics::POSITION_EPSILON ||
        std::fabs(body.GetAngle() - transform.rotation) > physics::POSITION_EPSILON) {
        body.SetTransform(b2Vec2{targetX, targetY}, transform.rotation);
    }

    if (rigidBody.bodyType != ecs::BodyType::Static) {
        b2Vec2 velocity{rigidBody.velocityX / physics::PPM, rigidBody.velocityY / physics::PPM};
        const float maxSpeed = std::max(0.0F, rigidBody.maxLinearSpeed / physics::PPM);
        const float speed = velocity.Length();
        if (speed > maxSpeed && speed > 0.0F) {
            velocity *= maxSpeed / speed;
            rigidBody.velocityX = velocity.x * physics::PPM;
            rigidBody.velocityY = velocity.y * physics::PPM;
        }
        body.SetLinearVelocity(velocity);
    }
    synchronizeCollider(entity, transform, body);
}

void PhysicsSystem::synchronizeCollider(ecs::Entity entity, const ecs::Transform& transform, b2Body& body) {
    if (!_registry->hasComponent<ecs::Collider2D>(entity)) {
        while (b2Fixture* fixture = body.GetFixtureList()) {
            body.DestroyFixture(fixture);
        }
        _colliderScales.erase(entity);
        return;
    }

    const b2Vec2 scale{std::fabs(transform.scaleX), std::fabs(transform.scaleY)};
    const auto scaleIt = _colliderScales.find(entity);
    const bool scaleChanged = scaleIt == _colliderScales.end() ||
                              std::fabs(scaleIt->second.x - scale.x) > physics::POSITION_EPSILON ||
                              std::fabs(scaleIt->second.y - scale.y) > physics::POSITION_EPSILON;
    if (body.GetFixtureList() != nullptr && !scaleChanged) {
        return;
    }

    while (b2Fixture* fixture = body.GetFixtureList()) {
        body.DestroyFixture(fixture);
    }

    const ecs::Collider2D& collider = _registry->getComponents<ecs::Collider2D>().get(entity);
    const bool hasValidDimensions = collider.shape == ecs::ColliderShape::Circle
                                        ? std::max(scale.x, scale.y) > physics::POSITION_EPSILON
                                        : scale.x > physics::POSITION_EPSILON && scale.y > physics::POSITION_EPSILON;
    if (!hasValidDimensions) {
        _colliderScales.insert_or_assign(entity, scale);
        return;
    }

    b2FixtureDef fixtureDef;
    fixtureDef.density = collider.density;
    fixtureDef.friction = collider.friction;
    fixtureDef.restitution = collider.restitution;
    fixtureDef.isSensor = collider.isSensor;
    fixtureDef.filter.categoryBits = collider.categoryBits;
    fixtureDef.filter.maskBits = collider.maskBits;

    if (collider.shape == ecs::ColliderShape::Circle) {
        b2CircleShape shape;
        shape.m_radius = collider.radius * std::max(scale.x, scale.y) / physics::PPM;
        fixtureDef.shape = &shape;
        body.CreateFixture(&fixtureDef);
        _colliderScales.insert_or_assign(entity, scale);
        return;
    }

    b2PolygonShape shape;
    shape.SetAsBox(collider.halfWidth * scale.x / physics::PPM, collider.halfHeight * scale.y / physics::PPM);
    fixtureDef.shape = &shape;
    body.CreateFixture(&fixtureDef);
    _colliderScales.insert_or_assign(entity, scale);
}

void PhysicsSystem::destroyBody(ecs::Entity entity) {
    const auto it = _bodies.find(entity);
    if (it == _bodies.end()) {
        return;
    }
    _world.DestroyBody(it->second);
    _bodies.erase(it);
    _colliderScales.erase(entity);
}

}  // namespace systems
