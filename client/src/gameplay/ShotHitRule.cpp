/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Gameplay rule destroying player shots and the enemies they hit.
*/

#include "gameplay/ShotHitRule.hpp"

#include "collision/GameCollisionEvents.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "prefabs/CollisionCategories.hpp"

namespace client::gameplay {

using rtype::server::collision::ShotHitEvent;

ShotHitRule::ShotHitRule(ecs::EventBus& bus, ecs::Registry& registry)
    : _bus(&bus),
      _registry(&registry),
      _token(bus.subscribe<ShotHitEvent>([this](const ShotHitEvent& hit) { onShotHit(hit); })) {}

ShotHitRule::~ShotHitRule() { _bus->unsubscribe<ShotHitEvent>(_token); }

void ShotHitRule::onShotHit(const ShotHitEvent& hit) {
    if (!hit.byPlayer || !_registry->isAlive(hit.shot)) {
        return;
    }
    if ((hit.targetCategory & ::prefabs::collision::ENEMY) != 0 && _registry->isAlive(hit.target)) {
        _registry->killEntity(hit.target);
        ++_destroyedEnemies;
    }
    _registry->killEntity(hit.shot);
}

}  // namespace client::gameplay
