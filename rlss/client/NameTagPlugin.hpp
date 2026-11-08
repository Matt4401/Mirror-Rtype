/**
 * @file NameTagPlugin.hpp
 * @brief Client plugin showing each player's name above their car.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "UiCanvas.hpp"
#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Entity.hpp"
#include "events/EventBus.hpp"
#include "tools/cvars/CVarRef.hpp"

namespace ecs::time {
struct Time;
}

namespace rlss::client {

/**
 * @brief Draws the name of each player above their car, on a dark label underlined in the player color.
 *
 * The names come from the server's `PlayerNames` messages (car network id → name), sent when a player joins, leaves or
 * renames. A label is built once its car has arrived in the snapshots (the names and the car may come in any order),
 * and rebuilt when the name changes. It keeps the same size on screen whatever the zoom, and follows the car above its
 * boost gauge. Leaving the match removes every label.
 */
class NameTagPlugin final : public engine::IPlugin {
  public:
    /**
     * @brief Gets plugin name.
     * @return String view "NameTagPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "NameTagPlugin"; }

    /**
     * @brief Declares the dependencies: the network and the replication, and the camera (and its script), which must
     * be final before the labels are placed on screen.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Subscribes to the received packets, the despawned cars and the match left.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Builds the missing labels, removes the stale ones, and places them above their car.
     * @param engine Host engine.
     * @param time Frame time.
     */
    void update(engine::Engine& engine, const ecs::time::Time& time) override;

    /**
     * @brief Unsubscribes and removes the labels.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

    /**
     * @brief Names known for the match, by car network id.
     * @return The names of the last `PlayerNames`.
     */
    [[nodiscard]] const std::unordered_map<std::uint32_t, std::string>& names() const noexcept { return _names; }

  private:
    /** @brief The label of one car. */
    struct Tag {
        /** @brief Car entity it follows. */
        ecs::Entity car = 0;
        /** @brief Name drawn. */
        std::string name;
        /** @brief Background and letters, around the point above the car. */
        UiCanvas canvas;
    };

    /**
     * @brief Removes every label.
     * @param engine Host engine.
     */
    void clearTags(engine::Engine& engine);

    /** @brief Names of the match, by car network id. */
    std::unordered_map<std::uint32_t, std::string> _names;
    /** @brief Font of the labels. */
    Typeface _typeface;
    /** @brief Labels drawn, by car network id. */
    std::unordered_map<std::uint32_t, Tag> _tags;
    /** @brief True between `MatchJoined` and `MatchLeft`: names received outside a match are late, and ignored. */
    bool _inMatch = false;
    /** @brief Subscription to `PacketReceived`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _packets;
    /** @brief Subscription to `MatchJoined`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _joined;
    /** @brief Subscription to `MatchLeft`, while initialized. */
    std::optional<ecs::EventBus::EventToken> _left;
    /** @brief `cl_nametags`: whether the labels are drawn; unset without a console. */
    tools::CVarRef<bool> _visible;
};

}  // namespace rlss::client
