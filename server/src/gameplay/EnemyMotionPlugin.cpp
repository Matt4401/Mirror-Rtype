#include "gameplay/EnemyMotionPlugin.hpp"

#include <cmath>
#include <numbers>

#include "components/Transform.hpp"
#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/time/Time.hpp"
#include "prefabs/EnemyMotion.hpp"

namespace rtype::server::gameplay {

void EnemyMotionPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) {
    engine.registry().runSystem<::prefabs::SineWave, ecs::Transform>(
        [&time](ecs::Entity /*entity*/, ::prefabs::SineWave& wave, ecs::Transform& transform) {
            wave.time += time.deltaTime;
            transform.y =
                wave.baseY + (std::sin(wave.time * wave.frequency * 2.0F * std::numbers::pi_v<float>) * wave.amplitude);
        });
}

}  // namespace rtype::server::gameplay
