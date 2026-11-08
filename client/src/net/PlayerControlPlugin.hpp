#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "events/EventBus.hpp"
#include "net/ReplicationEvents.hpp"

namespace client {

/**
 * @brief What the local player does in game: the replication leaves the inputs and the movement to the game.
 *
 * - Once `GameStarted` is received, sends the state of the `input::InputState` of player 0 as `ecs::net::LocalInput`,
 *   once per fixed step.
 * - The ship of this player is `ecs::Predicted`, moved by `InputApplied` like on the server; the other replicated
 *   entities are `ecs::Interpolated`.
 *
 * It draws nothing and needs no window, so it runs in tests.
 */
class PlayerControlPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "PlayerControlPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "PlayerControlPlugin"; }

    /**
     * @brief Declares the dependencies on the session and on the replication.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the session and replication events.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Sends the input of this step, once the game has started.
     * @param engine Host engine.
     * @param time Time of this step.
     */
    void fixedUpdate(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes from the events.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /**
     * @brief The ship of this player becomes `ecs::Predicted`, every other replicated entity `ecs::Interpolated`.
     * @param engine Host engine.
     * @param event The new entity.
     */
    void onEntitySpawned(engine::Engine& engine, const ecs::net::ReplicatedEntitySpawned& event) const;

    /** @brief Index of this player's ship, once the game has started. */
    std::optional<std::uint8_t> _playerIndex;
    /** @brief Sequence of the last input sent. */
    std::uint32_t _sequence = 0;
    /** @brief Subscription to `GameStarted`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _started;
    /** @brief Subscription to `ReplicatedEntitySpawned`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _spawned;
    /** @brief Subscription to `InputApplied`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _inputs;
};

}  // namespace client
