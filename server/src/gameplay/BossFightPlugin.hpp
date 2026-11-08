/**
 * @file BossFightPlugin.hpp
 * @brief Runs the boss fights of an R-Type level.
 */

#pragma once

#include <optional>
#include <string_view>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "prefabs/PrefabParams.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Spawns the boss of a `boss` event (`LevelEventPlayed`) and follows the fight.
 *
 * The boss is a replicated `boss_core`, driven by the event's Wren `script` if any (module `scripts/Boss` for
 * `scripts/Boss.wren`, class named after the file unless the event sets `class`). It flies in until its `stop_x`.
 * Publishes `BossFightStarted` for each boss, and `BossFightEnded` once no boss is left.
 *
 * The parts of the event (`parts.<n>.*`, at most `level::MAX_BOSS_PARTS`) are replicated `boss_part` entities,
 * children of the boss in the entity hierarchy: they move with it and are destroyed with it. A part with an unknown
 * `role` is skipped, with a warning.
 */
class BossFightPlugin final : public engine::IPlugin {
  public:
    /** @brief Creates the plugin. Boss scripts are read under the script root of the room's `ScriptSystem`. */
    BossFightPlugin() = default;

    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeBossFightPlugin"; }

    /**
     * @brief Requires `systems::PhysicsPlugin`, which moves the bosses, and `hierarchy::HierarchyPlugin`, which moves
     * their parts.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the played level events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Stops the bosses that reached their place, and ends the fight once no boss is left.
     * @param engine Host engine.
     * @param time Fixed step time.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the level events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief Spawns the replicated, possibly scripted, boss of a `boss` event and publishes `BossFightStarted`.
     * @param registry Room registry.
     * @param params Event parameters: `prefab`, `script`, `class` and the prefab parameters.
     */
    void startFight(ecs::Registry& registry, const prefabs::PrefabParams& params);

    /**
     * @brief Spawns the replicated parts of a boss, as children of it.
     * @param registry Room registry.
     * @param boss The boss, already placed.
     * @param params Event parameters, holding `parts.count` and the `parts.<n>.*` keys.
     */
    static void spawnParts(ecs::Registry& registry, ecs::Entity boss, const prefabs::PrefabParams& params);

    /** @brief Event bus of the room, set by `init()`. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Subscription to the played level events. */
    std::optional<ecs::EventBus::EventToken> _playedToken;
    /** @brief True from a `boss` event until no boss is left. */
    bool _fighting = false;
};

}  // namespace rtype::server::gameplay
