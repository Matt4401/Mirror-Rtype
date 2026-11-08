/**
 * @file TerrainPlugin.hpp
 * @brief Builds and scrolls the terrain of an R-Type level.
 */

#pragma once

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "ecs/time/Time.hpp"
#include "events/EventBus.hpp"
#include "prefabs/PrefabParams.hpp"

namespace rtype::server::gameplay {

/**
 * @brief Builds the terrain sections of the level and moves them at the scroll speed.
 *
 * A `terrain` event (`LevelEventPlayed`) starts a section along the top or bottom edge. It is built block by block
 * (`game::TERRAIN_BLOCK_WIDTH` wide, `KIND_OBSTACLE`, `TerrainBlock`) as it enters the screen, and the blocks move left
 * at the speed of `timeline::TimelinePlugin`, so they stop whenever the scroll is held. `LevelStarted` drops the
 * sections still entering.
 */
class TerrainPlugin final : public engine::IPlugin {
  public:
    /** @brief Plugin name. */
    [[nodiscard]] std::string_view name() const noexcept override { return "RTypeTerrainPlugin"; }

    /**
     * @brief Requires `systems::PhysicsPlugin`, which moves the blocks, and `timeline::TimelinePlugin`, which gives the
     * scroll speed.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the level events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Builds the blocks entering the screen and moves every block at the scroll speed.
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
    /** @brief A terrain section still entering the screen: what is left to build. */
    struct Section {
        /** @brief Center ordinate of its blocks. */
        float y = 0.0F;
        /** @brief Height of its blocks, a multiple of `game::TERRAIN_HEIGHT_UNIT`. */
        float height = 0.0F;
        /** @brief Blocks left to build. */
        std::size_t blocksLeft = 0;
        /** @brief Last block built, to place the next one right after it; none before the first. */
        std::optional<ecs::Entity> lastBlock;
    };

    /**
     * @brief Queues a section from the parameters of a `terrain` event.
     * @param params `side`, `height` and `length` of the section.
     */
    void startSection(const prefabs::PrefabParams& params);

    /** @brief Event bus of the room, set by `init()`. */
    ecs::EventBus* _bus = nullptr;
    /** @brief Subscription to the played level events. */
    std::optional<ecs::EventBus::EventToken> _playedToken;
    /** @brief Subscription to the level starts. */
    std::optional<ecs::EventBus::EventToken> _startedToken;
    /** @brief Sections still entering the screen. */
    std::vector<Section> _sections;
};

}  // namespace rtype::server::gameplay
