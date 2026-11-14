#include "gameplay/Gameplay.hpp"

#include <memory>

#include "core/Engine.hpp"
#include "gameplay/BossFightPlugin.hpp"
#include "gameplay/CleanupPlugin.hpp"
#include "gameplay/CombatPlugin.hpp"
#include "gameplay/EnemyFirePlugin.hpp"
#include "gameplay/EnemyMotionPlugin.hpp"
#include "gameplay/LevelDirectorPlugin.hpp"
#include "gameplay/RespawnPlugin.hpp"
#include "gameplay/TerrainPlugin.hpp"
#include "level/Level.hpp"

namespace rtype::server::gameplay {

void addGameplayPlugins(engine::Engine& engine, const std::shared_ptr<const level::Level>& level) {
    // Each plugin has one job; they talk through the events of GameplayEvents.hpp, never through direct calls.
    engine.addPlugin<LevelDirectorPlugin>(level);
    engine.addPlugin<CombatPlugin>();
    engine.addPlugin<BossFightPlugin>();
    engine.addPlugin<RespawnPlugin>();
    engine.addPlugin<TerrainPlugin>();
    engine.addPlugin<CleanupPlugin>();
    engine.addPlugin<EnemyFirePlugin>();
    engine.addPlugin<EnemyMotionPlugin>();
}

}  // namespace rtype::server::gameplay
