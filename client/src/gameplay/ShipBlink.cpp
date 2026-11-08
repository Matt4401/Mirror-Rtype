/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Makes the invulnerable ships blink.
*/

#include "gameplay/ShipBlink.hpp"

#include <cmath>
#include <cstdint>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"

namespace client::gameplay {

std::uint8_t blinkAlpha(const std::uint8_t flags, const float seconds) noexcept {
    // An eliminated ship is out of the game: it is not drawn any more.
    if ((flags & rtype::game::FLAG_ELIMINATED) != 0) {
        return 0;
    }
    if ((flags & rtype::game::FLAG_INVULNERABLE) == 0) {
        return OPAQUE_ALPHA;
    }
    const float phase = std::fmod(seconds, BLINK_PERIOD);
    return phase < BLINK_PERIOD / 2.0F ? OPAQUE_ALPHA : BLINK_FADED_ALPHA;
}

void blinkInvulnerableShips(ecs::Registry& registry, const float seconds) {
    registry.runSystem<ecs::Replicated, rtype::game::EntityNetState, ecs::Renderable>(
        [seconds](ecs::Entity /*entity*/, const ecs::Replicated& replicated, const rtype::game::EntityNetState& state,
                  ecs::Renderable& renderable) {
            if (replicated.kind == rtype::game::KIND_PLAYER_SHIP) {
                renderable.tint.a = blinkAlpha(state.flags, seconds);
            }
        });
}

}  // namespace client::gameplay
