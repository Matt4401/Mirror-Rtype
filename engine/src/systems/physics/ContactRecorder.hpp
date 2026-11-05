#pragma once

#include <box2d/b2_world_callbacks.h>

#include <vector>

#include "systems/physics/PhysicsEvents.hpp"

namespace systems {

/**
 * @brief Records the contacts that begin during a Box2D step.
 *
 * The world stays locked until the step ends, so nothing can be done with a contact while Box2D reports it. Bodies must
 * store their entity in `b2BodyDef::userData.pointer`.
 */
class ContactRecorder final : public b2ContactListener {
  public:
    /**
     * @brief Stores the two entities of a new contact.
     * @param contact Contact that just began.
     */
    void BeginContact(b2Contact* contact) override;

    /** @brief Contacts that began since the last time this list was cleared. */
    std::vector<CollisionEvent> began;
};

}  // namespace systems
