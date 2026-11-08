/**
 * @file TimelineEvent.hpp
 * @brief One event of a Timeline: a position, a type and its parameters.
 */

#pragma once

#include <string>

#include "ecs/ParamBag.hpp"

namespace timeline {

/**
 * @brief Something that happens when a timeline cursor reaches a position.
 *
 * The engine gives no meaning to `position` or `type`: the game decides what the cursor measures (scroll distance,
 * seconds, beats...) and what each type does. `params` carries the event data; for a spawn it is handed to the prefab
 * as is.
 */
struct TimelineEvent {
    /** @brief Cursor position at which the event fires. */
    float position = 0.0F;

    /** @brief Event type, e.g. `"spawn"`; interpreted by whoever handles the event. */
    std::string type;

    /** @brief Event data, e.g. the prefab name and its parameters. */
    ecs::ParamBag params;
};

}  // namespace timeline
