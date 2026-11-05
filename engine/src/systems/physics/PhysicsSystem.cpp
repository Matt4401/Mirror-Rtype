#include "systems/physics/PhysicsSystem.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_math.h>
#include <box2d/b2_polygon_shape.h>
#include <box2d/b2_world.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"
#include "ecs/ComponentType.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/PhysicsEvents.hpp"
#include "systems/physics/PhysicsConstants.hpp"
#include "systems/physics/detail/Box2DContactListener.hpp"

namespace systems {

namespace {

using physics::detail::BodySerial;
using physics::detail::Box2DContactListener;
using physics::detail::ContactSide;
using physics::detail::PendingContact;

/** @brief Converts an ECS rigid-body mode to its Box2D equivalent. */
b2BodyType toBox2DBodyType(ecs::BodyType bodyType) {
    switch (bodyType) {
        case ecs::BodyType::Static:
            return b2_staticBody;
        case ecs::BodyType::Kinematic:
            return b2_kinematicBody;
        case ecs::BodyType::Dynamic:
            return b2_dynamicBody;
    }
    return b2_staticBody;
}

/** @brief Box2D body owned by an entity. */
struct BodyRecord {
    /** @brief Box2D body handle. */
    b2Body* body = nullptr;
    /** @brief Serial given to the body at creation. */
    BodySerial serial = 0;
};

}  // namespace

/** @brief Box2D-backed state of a PhysicsSystem. */
struct PhysicsSystem::Impl {
    Impl(ecs::Registry& registry, ecs::EventBus& bus);
    ~Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    void update(float fixedDt);
    void createBody(ecs::Entity entity, const ecs::Transform& transform, const ecs::RigidBody2D& rigidBody);
    void synchronizeBody(ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody);
    void synchronizeCollider(ecs::Entity entity, const ecs::Transform& transform, b2Body& body);
    static void destroyFixtures(b2Body& body);
    void destroyBody(ecs::Entity entity);
    [[nodiscard]] bool isCurrent(const ContactSide& side) const;
    void publishContacts();

    ecs::Registry* registry;
    ecs::EventBus* bus;
    b2World world;
    Box2DContactListener listener;
    std::vector<PendingContact> dispatch;
    std::unordered_map<ecs::Entity, BodyRecord> bodies;
    std::unordered_map<ecs::Entity, b2Vec2> colliderScales;
    BodySerial nextSerial = 1;
    std::size_t destroyListenerId;
    std::size_t componentRemoveListenerId;
};

PhysicsSystem::Impl::Impl(ecs::Registry& registry, ecs::EventBus& bus)
    : registry(&registry),
      bus(&bus),
      world(b2Vec2_zero),
      destroyListenerId(registry.addDestroyListener([this](ecs::Entity entity) { destroyBody(entity); })),
      componentRemoveListenerId(
          registry.addComponentRemoveListener([this](ecs::Entity entity, ecs::ComponentTypeId componentType) {
              if (componentType == ecs::getComponentTypeId<ecs::RigidBody2D>() ||
                  componentType == ecs::getComponentTypeId<ecs::Transform>()) {
                  destroyBody(entity);
              }
          })) {
    dispatch.reserve(physics::INITIAL_CONTACT_CAPACITY);
    world.SetContactListener(&listener);
}

PhysicsSystem::Impl::~Impl() {
    registry->removeDestroyListener(destroyListenerId);
    registry->removeComponentRemoveListener(componentRemoveListenerId);
    listener.setSuppressed(true);
    world.SetContactListener(nullptr);
}

void PhysicsSystem::Impl::update(float fixedDt) {
    if (fixedDt <= 0.0F) {
        return;
    }

    registry->runSystem<ecs::Transform, ecs::RigidBody2D>(
        [this](ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
            synchronizeBody(entity, transform, rigidBody);
        });

    world.Step(fixedDt, physics::VELOCITY_ITERATIONS, physics::POSITION_ITERATIONS);

    registry->runSystem<ecs::Transform, ecs::RigidBody2D>(
        [this](ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
            if (rigidBody.bodyType == ecs::BodyType::Static) {
                return;
            }

            const auto it = bodies.find(entity);
            if (it == bodies.end()) {
                return;
            }

            const b2Body& body = *it->second.body;
            const b2Vec2 position = body.GetPosition();
            const b2Vec2 velocity = body.GetLinearVelocity();
            transform.x = position.x * physics::PPM;
            transform.y = position.y * physics::PPM;
            transform.rotation = body.GetAngle();
            rigidBody.velocityX = velocity.x * physics::PPM;
            rigidBody.velocityY = velocity.y * physics::PPM;
        });

    publishContacts();
}

void PhysicsSystem::Impl::createBody(ecs::Entity entity, const ecs::Transform& transform,
                                     const ecs::RigidBody2D& rigidBody) {
    b2BodyDef bodyDef;
    bodyDef.type = toBox2DBodyType(rigidBody.bodyType);
    bodyDef.position.Set(transform.x / physics::PPM, transform.y / physics::PPM);
    bodyDef.angle = transform.rotation;
    bodyDef.linearVelocity.Set(rigidBody.velocityX / physics::PPM, rigidBody.velocityY / physics::PPM);
    bodyDef.linearDamping = rigidBody.linearDamping;
    bodyDef.angularDamping = rigidBody.angularDamping;
    bodyDef.fixedRotation = rigidBody.fixedRotation;
    bodyDef.bullet = rigidBody.bullet;
    bodyDef.userData.pointer = nextSerial;

    b2Body* body = world.CreateBody(&bodyDef);
    bodies.insert_or_assign(entity, BodyRecord{.body = body, .serial = nextSerial});
    ++nextSerial;
    synchronizeCollider(entity, transform, *body);
}

void PhysicsSystem::Impl::synchronizeBody(ecs::Entity entity, ecs::Transform& transform, ecs::RigidBody2D& rigidBody) {
    auto it = bodies.find(entity);
    if (it == bodies.end()) {
        createBody(entity, transform, rigidBody);
        return;
    }

    b2Body& body = *it->second.body;
    body.SetType(toBox2DBodyType(rigidBody.bodyType));
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

void PhysicsSystem::Impl::destroyFixtures(b2Body& body) {
    while (b2Fixture* fixture = body.GetFixtureList()) {
        body.DestroyFixture(fixture);
    }
}

void PhysicsSystem::Impl::synchronizeCollider(ecs::Entity entity, const ecs::Transform& transform, b2Body& body) {
    if (!registry->hasComponent<ecs::Collider2D>(entity)) {
        destroyFixtures(body);
        colliderScales.erase(entity);
        return;
    }

    const b2Vec2 scale{std::fabs(transform.scaleX), std::fabs(transform.scaleY)};
    const auto scaleIt = colliderScales.find(entity);
    const bool scaleChanged = scaleIt == colliderScales.end() ||
                              std::fabs(scaleIt->second.x - scale.x) > physics::POSITION_EPSILON ||
                              std::fabs(scaleIt->second.y - scale.y) > physics::POSITION_EPSILON;
    if (body.GetFixtureList() != nullptr && !scaleChanged) {
        return;
    }

    destroyFixtures(body);

    const ecs::Collider2D& collider = registry->getComponents<ecs::Collider2D>().get(entity);
    const bool hasValidDimensions = collider.shape == ecs::ColliderShape::Circle
                                        ? std::max(scale.x, scale.y) > physics::POSITION_EPSILON
                                        : scale.x > physics::POSITION_EPSILON && scale.y > physics::POSITION_EPSILON;
    if (!hasValidDimensions) {
        colliderScales.insert_or_assign(entity, scale);
        return;
    }

    b2FixtureDef fixtureDef;
    fixtureDef.density = collider.density;
    fixtureDef.friction = collider.friction;
    fixtureDef.restitution = collider.restitution;
    fixtureDef.isSensor = collider.isSensor;
    fixtureDef.filter.categoryBits = collider.categoryBits;
    fixtureDef.filter.maskBits = collider.maskBits;
    fixtureDef.userData.pointer = static_cast<std::uintptr_t>(entity);

    b2CircleShape circle;
    b2PolygonShape box;
    if (collider.shape == ecs::ColliderShape::Circle) {
        circle.m_radius = collider.radius * std::max(scale.x, scale.y) / physics::PPM;
        fixtureDef.shape = &circle;
    } else {
        box.SetAsBox(collider.halfWidth * scale.x / physics::PPM, collider.halfHeight * scale.y / physics::PPM);
        fixtureDef.shape = &box;
    }
    body.CreateFixture(&fixtureDef);
    colliderScales.insert_or_assign(entity, scale);
}

void PhysicsSystem::Impl::destroyBody(ecs::Entity entity) {
    const auto it = bodies.find(entity);
    if (it == bodies.end()) {
        return;
    }
    listener.setSuppressed(true);
    world.DestroyBody(it->second.body);
    listener.setSuppressed(false);
    bodies.erase(it);
    colliderScales.erase(entity);
}

bool PhysicsSystem::Impl::isCurrent(const ContactSide& side) const {
    if (!registry->isAlive(side.entity)) {
        return false;
    }
    const auto it = bodies.find(side.entity);
    return it != bodies.end() && it->second.serial == side.serial;
}

void PhysicsSystem::Impl::publishContacts() {
    listener.swapPending(dispatch);
    for (const PendingContact& contact : dispatch) {
        if (!isCurrent(contact.a) || !isCurrent(contact.b)) {
            continue;
        }
        if (contact.begin) {
            bus->publish(events::CollisionBeginEvent{
                .entityA = contact.a.entity,
                .entityB = contact.b.entity,
                .categoryA = contact.a.category,
                .categoryB = contact.b.category,
                .isSensor = contact.isSensor,
                .hasContactPoint = contact.hasContactPoint,
                .normalX = contact.normalX,
                .normalY = contact.normalY,
                .pointX = contact.pointX,
                .pointY = contact.pointY,
            });
        } else {
            bus->publish(events::CollisionEndEvent{
                .entityA = contact.a.entity,
                .entityB = contact.b.entity,
                .categoryA = contact.a.category,
                .categoryB = contact.b.category,
                .isSensor = contact.isSensor,
            });
        }
    }
    dispatch.clear();
}

PhysicsSystem::PhysicsSystem(ecs::Registry& registry, ecs::EventBus& bus)
    : _impl(std::make_unique<Impl>(registry, bus)) {}

PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::update(float fixedDt) { _impl->update(fixedDt); }

std::size_t PhysicsSystem::bodyCount() const noexcept { return _impl->bodies.size(); }

}  // namespace systems
