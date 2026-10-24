#pragma once

namespace ecs {

/**
 * @brief Marks the entity whose `Transform` positioned sounds are heard from (usually the camera or the player).
 *
 * The first active listener found is used; without any, positioned sounds play centered at full volume.
 */
struct AudioListener {
    /** @brief Whether this entity can be the listener. */
    bool active = true;
};

}  // namespace ecs
