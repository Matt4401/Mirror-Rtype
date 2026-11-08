/**
 * @file LobbyScenes.hpp
 * @brief Scenes of the R-Type lobby: the list of rooms, and the room waiting for its players.
 */

#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "ecs/Entity.hpp"
#include "protocol/Messages.hpp"
#include "scene/IScene.hpp"

namespace client {

/** @brief List of rooms: join one, create one, refresh, or go back to the title (disconnects). */
class LobbyScene final : public scene::IScene {
  public:
    /** @brief Rows shown at once. */
    static constexpr std::size_t VISIBLE_ROWS = 6;

    /**
     * @brief Gets the scene name.
     * @return "Lobby".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Lobby"; }

    /**
     * @brief Spawns the list from the last rooms received, and asks for a fresh list.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Replaces the rows of the list.
     * @param context Scene context.
     * @param list Rooms of the server.
     */
    void showRooms(scene::SceneContext& context, const rtype::protocol::LobbyList& list);

    /** @brief Scroll list holding one row per room. */
    ecs::Entity _list = ecs::NULL_ENTITY;
    /** @brief Line shown when there is no room. */
    ecs::Entity _empty = ecs::NULL_ENTITY;
    /** @brief "Create" button. */
    ecs::Entity _create = ecs::NULL_ENTITY;
    /** @brief Current rows. */
    std::vector<ecs::Entity> _rows;
};

/** @brief A room waiting for its players: who is ready, toggle ready, leave. Switches to the game at `GameStart`. */
class RoomScene final : public scene::IScene {
  public:
    /**
     * @brief Gets the scene name.
     * @return "Room".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "Room"; }

    /**
     * @brief Spawns the room from the last update received.
     * @param context Scene context.
     */
    void onEnter(scene::SceneContext& context) override;

  private:
    /**
     * @brief Shows the players and their ready states.
     * @param context Scene context.
     * @param room The room.
     */
    void showRoom(scene::SceneContext& context, const rtype::protocol::LobbyUpdate& room);

    /** @brief Title line, with the room number. */
    ecs::Entity _title = ecs::NULL_ENTITY;
    /** @brief One line per player slot. */
    std::vector<ecs::Entity> _slots;
    /** @brief "Ready" button. */
    ecs::Entity _readyButton = ecs::NULL_ENTITY;
    /** @brief Ready state last requested. */
    bool _ready = false;
};

}  // namespace client
