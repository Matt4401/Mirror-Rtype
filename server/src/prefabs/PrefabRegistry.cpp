#include "prefabs/PrefabRegistry.hpp"

#include "prefabs/PrefabManager.hpp"
#include "prefabs/alien_scout/AlienScout.hpp"
#include "prefabs/missile/Missile.hpp"
#include "prefabs/obstacle/Obstacle.hpp"
#include "prefabs/player_bullet/PlayerBullet.hpp"
#include "prefabs/power_up/PowerUp.hpp"

namespace prefabs {

void registerRtypePrefabs(PrefabManager& manager) {
    manager.registerPrefab(MISSILE_PREFAB, buildMissile);
    manager.registerPrefab(PLAYER_BULLET_PREFAB, buildPlayerBullet);
    manager.registerPrefab(ALIEN_SCOUT_PREFAB, buildAlienScout);
    manager.registerPrefab(OBSTACLE_PREFAB, buildObstacle);
    manager.registerPrefab(POWER_UP_PREFAB, buildPowerUp);
}

}  // namespace prefabs
