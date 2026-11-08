#include "gameplay/PlayerLives.hpp"

#include <cstdint>

#include "components/Renderable.hpp"
#include "components/Replicated.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/ReplicationCodec.hpp"

namespace client::gameplay {

ecs::TextureRect idleShipFrame(const ShipIconSheet& sheet, const std::uint8_t playerIndex) noexcept {
    return {.x = (sheet.frames / 2) * sheet.frameWidth,
            .y = static_cast<int>(playerIndex) * sheet.frameHeight,
            .width = sheet.frameWidth,
            .height = sheet.frameHeight};
}

OwnShipStatus ownShipStatus(const ecs::Registry& registry, const std::uint8_t playerIndex) {
    OwnShipStatus status;
    const auto* replicated = registry.findComponents<ecs::Replicated>();
    if (replicated == nullptr) {
        return status;
    }
    for (const ecs::Entity entity : replicated->getDenseEntities()) {
        if (replicated->get(entity).kind != rtype::game::KIND_PLAYER_SHIP ||
            !registry.hasComponent<rtype::game::EntityNetState>(entity)) {
            continue;
        }
        const auto& state = registry.getComponents<rtype::game::EntityNetState>().get(entity);
        if (state.subtype == playerIndex) {
            status.lives = state.health;
            status.eliminated = (state.flags & rtype::game::FLAG_ELIMINATED) != 0;
            break;
        }
    }
    return status;
}

}  // namespace client::gameplay
