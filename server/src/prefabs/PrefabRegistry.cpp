#include "prefabs/PrefabRegistry.hpp"

#include "prefabs/PrefabManager.hpp"
#include "prefabs/alien_gunner/AlienGunner.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/boss_core/BossCore.hpp"
#include "prefabs/boss_part/BossPart.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/player_ship/PlayerShip.hpp"
#include "prefabs/power_up/PowerUp.hpp"

namespace prefabs {

void registerRtypePrefabs(PrefabManager& manager) {
    manager.registerPrefab(MISSILE_PREFAB, buildMissile);
    manager.registerPrefab(PLAYER_BULLET_PREFAB, buildPlayerBullet);
    manager.registerPrefab(ALIEN_SCOUT_PREFAB, buildAlienScout);
    manager.registerPrefab(ALIEN_GUNNER_PREFAB, buildAlienGunner);
    manager.registerPrefab(OBSTACLE_PREFAB, buildObstacle);
    manager.registerPrefab(BOSS_CORE_PREFAB, buildBossCore);
    manager.registerPrefab(BOSS_PART_PREFAB, buildBossPart);
    manager.registerPrefab(POWER_UP_PREFAB, buildPowerUp);
    manager.registerPrefab(PLAYER_SHIP_PREFAB, buildPlayerShip);
}

}  // namespace prefabs
