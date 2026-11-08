#include "scenes/LobbyScenes.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "net/RTypeNetworkPlugin.hpp"
#include "net/SessionEvents.hpp"
#include "protocol/Messages.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneManager.hpp"
#include "scenes/GameScenes.hpp"
#include "scenes/MenuKit.hpp"
#include "scenes/MenuScenes.hpp"
#include "ui/components/UiAnchor.hpp"
#include "ui/components/UiFocusable.hpp"
#include "ui/components/UiRect.hpp"
#include "ui/components/UiScrollList.hpp"

namespace client {

namespace protocol = rtype::protocol;

namespace {

/** @brief Height of one row of the list of rooms. */
constexpr float ROW_HEIGHT = 46.0F;
/** @brief Width of the list of rooms. */
constexpr float LIST_WIDTH = 480.0F;
/** @brief Prefix of the action of a room row; the room id follows. */
constexpr std::string_view JOIN_PREFIX = "lobby.join.";

/**
 * @brief Gets the session of the client.
 * @param context Scene context.
 * @return The session plugin.
 */
RTypeNetworkPlugin& sessionOf(scene::SceneContext& context) { return context.engine().getPlugin<RTypeNetworkPlugin>(); }

/**
 * @brief Text of a room row.
 * @param room The room.
 * @return "ROOM 3    2/4    WAITING".
 */
std::string describe(const protocol::RoomEntry& room) {
    return "ROOM " + std::to_string(room.roomId) + "    " + std::to_string(room.playerCount) + "/" +
           std::to_string(room.maxPlayers) + "    " +
           (room.state == protocol::RoomState::InGame ? "IN GAME" : "WAITING");
}

/**
 * @brief Goes back to the connect screen when the connection is lost.
 * @param context Scene context.
 */
void returnOnSessionClosed(scene::SceneContext& context) {
    context.subscribe<SessionClosed>(
        [&context](const SessionClosed& /*event*/) { context.manager().switchTo<ConnectScene>(); });
}

}  // namespace

// --- Lobby -----------------------------------------------------------------------------------------------------------

void LobbyScene::onEnter(scene::SceneContext& context) {
    ecs::Registry& registry = context.registry();
    _rows.clear();
    const ecs::Entity box = menu::panel(context, 560.0F, 520.0F);
    (void)menu::label(context, box, "LOBBY", 24.0F, 34.0F, menu::GOLD);

    _list = context.spawn();
    registry.addComponent(_list, ui::UiAnchor{.anchorX = 0.5F, .offsetY = 80.0F, .pivotX = 0.5F});
    registry.addComponent(_list, ui::UiRect{.width = LIST_WIDTH,
                                            .height = ROW_HEIGHT * static_cast<float>(VISIBLE_ROWS),
                                            .fillColor = {.r = 0, .g = 0, .b = 0, .a = 60}});
    registry.addComponent(_list, ui::UiScrollList{.visibleCount = VISIBLE_ROWS, .itemHeight = ROW_HEIGHT});
    hierarchy::setParent(registry, _list, box);
    _empty = menu::label(context, box, "", 200.0F, 20.0F, menu::MUTED);

    _create = menu::button(context, box, "CREATE A ROOM", "lobby.create", 370.0F, 100);
    (void)menu::button(context, box, "REFRESH", "lobby.refresh", 420.0F, 101);
    (void)menu::button(context, box, "BACK", "lobby.back", 470.0F, 102);

    const auto back = [&context] {
        context.bus().publish(DisconnectRequest{});
        context.manager().switchTo<TitleScene>();
    };
    menu::onBack(context, back);
    menu::onClick(context, [&context, back](std::string_view action) {
        if (action == "lobby.create") {
            context.bus().publish(CreateRoomRequest{});
        } else if (action == "lobby.refresh") {
            context.bus().publish(RefreshRoomsRequest{});
        } else if (action == "lobby.back") {
            back();
        } else if (action.starts_with(JOIN_PREFIX)) {
            std::uint32_t room = 0;
            const std::string_view id = action.substr(JOIN_PREFIX.size());
            if (std::from_chars(std::to_address(id.begin()), std::to_address(id.end()), room).ec == std::errc{}) {
                context.bus().publish(JoinRoomRequest{.roomId = room});
            }
        }
    });
    context.subscribe<RoomListUpdated>(
        [this, &context](const RoomListUpdated& event) { showRooms(context, event.list); });
    context.subscribe<RoomUpdated>(
        [&context](const RoomUpdated& /*event*/) { context.manager().switchTo<RoomScene>(); });
    returnOnSessionClosed(context);

    showRooms(context, sessionOf(context).rooms());
    menu::focus(registry, _rows.empty() ? _create : _rows.front());
    context.bus().publish(RefreshRoomsRequest{});
}

void LobbyScene::showRooms(scene::SceneContext& context, const protocol::LobbyList& list) {
    ecs::Registry& registry = context.registry();
    for (const ecs::Entity row : _rows) {
        registry.killEntity(row);
    }
    _rows.clear();
    int order = 0;
    for (const protocol::RoomEntry& room : list.rooms) {
        const bool open = room.state == protocol::RoomState::Waiting && room.playerCount < room.maxPlayers;
        const std::string action = open ? std::string(JOIN_PREFIX) + std::to_string(room.roomId) : "lobby.none";
        const ecs::Entity row = menu::button(context, _list, describe(room), action, 3.0F, order++, LIST_WIDTH - 20.0F);
        if (!open) {
            registry.removeComponent<ui::UiFocusable>(row);
        }
        _rows.push_back(row);
    }
    menu::setText(registry, _empty, _rows.empty() ? "NO ROOM YET: CREATE ONE" : "");
}

// --- Room ------------------------------------------------------------------------------------------------------------

void RoomScene::onEnter(scene::SceneContext& context) {
    _ready = false;
    _slots.clear();
    const ecs::Entity box = menu::panel(context, 460.0F, 440.0F);
    _title = menu::label(context, box, "ROOM", 24.0F, 34.0F, menu::GOLD);
    (void)menu::label(context, box, "THE GAME STARTS WHEN EVERYONE IS READY", 70.0F, 16.0F, menu::MUTED);
    for (std::size_t slot = 0; slot < protocol::MAX_PLAYERS; ++slot) {
        _slots.push_back(menu::label(context, box, "", 110.0F + (36.0F * static_cast<float>(slot)), 22.0F));
    }
    _readyButton = menu::button(context, box, "READY", "room.ready", 300.0F, 0);
    (void)menu::button(context, box, "LEAVE", "room.leave", 355.0F, 1);
    menu::focus(context.registry(), _readyButton);

    const auto leave = [&context] { context.bus().publish(LeaveRoomRequest{}); };
    menu::onBack(context, leave);
    menu::onClick(context, [this, &context, leave](std::string_view action) {
        if (action == "room.ready") {
            _ready = !_ready;
            menu::setText(context.registry(), _readyButton, _ready ? "NOT READY" : "READY");
            context.bus().publish(SetReadyRequest{.ready = _ready});
        } else if (action == "room.leave") {
            leave();
        }
    });
    context.subscribe<RoomUpdated>([this, &context](const RoomUpdated& event) { showRoom(context, event.room); });
    context.subscribe<RoomLeft>([&context](const RoomLeft& /*event*/) { context.manager().switchTo<LobbyScene>(); });
    context.subscribe<GameStarted>(
        [&context](const GameStarted& /*event*/) { context.manager().switchTo<GameScene>(); });
    returnOnSessionClosed(context);

    if (const auto& room = sessionOf(context).room()) {
        showRoom(context, *room);
    }
}

void RoomScene::showRoom(scene::SceneContext& context, const protocol::LobbyUpdate& room) {
    ecs::Registry& registry = context.registry();
    menu::setText(registry, _title, "ROOM " + std::to_string(room.roomId));
    for (std::size_t slot = 0; slot < _slots.size(); ++slot) {
        if (slot >= room.slots.size()) {
            menu::setText(registry, _slots[slot], "- EMPTY -");
            menu::setColor(registry, _slots[slot], menu::MUTED);
            continue;
        }
        const bool ready = room.slots[slot].ready;
        menu::setText(registry, _slots[slot],
                      "PLAYER " + std::to_string(slot + 1) + (ready ? "    READY" : "    NOT READY"));
        menu::setColor(registry, _slots[slot], ready ? menu::GOOD : menu::TEXT);
    }
}

}  // namespace client
