#include "systems/physics/detail/Box2DContactListener.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_collision.h>
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_math.h>

#include <cstddef>
#include <span>
#include <vector>

#include "ecs/Entity.hpp"
#include "systems/physics/PhysicsConstants.hpp"

namespace physics::detail {

namespace {

ContactSide sideOf(const b2Fixture& fixture) {
    return ContactSide{
        .entity = static_cast<ecs::Entity>(fixture.GetUserData().pointer),
        .serial = fixture.GetBody()->GetUserData().pointer,
        .category = fixture.GetFilterData().categoryBits,
    };
}

}  // namespace

Box2DContactListener::Box2DContactListener() { _pending.reserve(physics::INITIAL_CONTACT_CAPACITY); }

void Box2DContactListener::BeginContact(b2Contact* contact) {
    if (contact != nullptr) {
        record(*contact, true);
    }
}

void Box2DContactListener::EndContact(b2Contact* contact) {
    if (contact != nullptr) {
        record(*contact, false);
    }
}

void Box2DContactListener::swapPending(std::vector<PendingContact>& out) noexcept {
    _pending.swap(out);
    _pending.clear();
}

void Box2DContactListener::record(b2Contact& contact, bool begin) {
    if (_suppressed) {
        return;
    }
    const b2Fixture& fixtureA = *contact.GetFixtureA();
    const b2Fixture& fixtureB = *contact.GetFixtureB();
    PendingContact pending{
        .a = sideOf(fixtureA),
        .b = sideOf(fixtureB),
        .isSensor = fixtureA.IsSensor() || fixtureB.IsSensor(),
        .begin = begin,
    };

    // Box2D computes the manifold before calling BeginContact; sensors have none (pointCount == 0).
    const int pointCount = contact.GetManifold()->pointCount;
    if (begin && pointCount > 0) {
        b2WorldManifold manifold{};
        contact.GetWorldManifold(&manifold);
        b2Vec2 point = b2Vec2_zero;
        for (const b2Vec2& manifoldPoint : std::span(manifold.points).first(static_cast<std::size_t>(pointCount))) {
            point += manifoldPoint;
        }
        point *= physics::PPM / static_cast<float>(pointCount);
        pending.normalX = manifold.normal.x;
        pending.normalY = manifold.normal.y;
        pending.pointX = point.x;
        pending.pointY = point.y;
        pending.hasContactPoint = true;
    }
    _pending.push_back(pending);
}

}  // namespace physics::detail
