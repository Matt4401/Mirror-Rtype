#include "systems/physics/ContactRecorder.hpp"

#include <box2d/b2_body.h>
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>

#include "ecs/Entity.hpp"

namespace systems {

void ContactRecorder::BeginContact(b2Contact* contact) {
    const auto entityOf = [](b2Fixture* fixture) {
        return static_cast<ecs::Entity>(fixture->GetBody()->GetUserData().pointer);
    };
    began.push_back({.a = entityOf(contact->GetFixtureA()), .b = entityOf(contact->GetFixtureB())});
}

}  // namespace systems
