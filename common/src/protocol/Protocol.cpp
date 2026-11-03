#include "protocol/Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "protocol/Messages.hpp"
#include "rtype.pb.h"

namespace rtype::protocol {

namespace {

void fill(pb::Envelope& envelope, const Hello& message) { envelope.mutable_hello()->set_version(message.version); }

void fill(pb::Envelope& envelope, const Welcome& message) { envelope.mutable_welcome()->set_udp_port(message.udpPort); }

void fill(pb::Envelope& envelope, const LobbyListRequest& /*message*/) { envelope.mutable_lobby_list_request(); }

void fill(pb::Envelope& envelope, const LobbyList& message) {
    auto* out = envelope.mutable_lobby_list();
    out->mutable_rooms()->Reserve(static_cast<int>(message.rooms.size()));
    for (const auto& [roomId, playerCount, maxPlayers, state] : message.rooms) {
        auto* room = out->add_rooms();
        room->set_room_id(roomId);
        room->set_player_count(playerCount);
        room->set_max_players(maxPlayers);
        room->set_state(static_cast<pb::RoomState>(state));
    }
}

void fill(pb::Envelope& envelope, const LobbyCreate& /*message*/) { envelope.mutable_lobby_create(); }

void fill(pb::Envelope& envelope, const LobbyJoin& message) {
    envelope.mutable_lobby_join()->set_room_id(message.roomId);
}

void fill(pb::Envelope& envelope, const LobbyReady& message) {
    envelope.mutable_lobby_ready()->set_ready(message.ready);
}

void fill(pb::Envelope& envelope, const LobbyUpdate& message) {
    auto* out = envelope.mutable_lobby_update();
    out->set_room_id(message.roomId);
    for (const auto& [clientId, ready] : message.slots) {
        auto* slot = out->add_slots();
        slot->set_client_id(clientId);
        slot->set_ready(ready);
    }
}

void fill(pb::Envelope& envelope, const GameStart& message) {
    auto* out = envelope.mutable_game_start();
    out->set_room_id(message.roomId);
    out->set_player_index(message.playerIndex);
    out->set_player_count(message.playerCount);
}

void fill(pb::Envelope& envelope, const GameOver& message) {
    auto* out = envelope.mutable_game_over();
    out->set_reason(static_cast<pb::GameOverReason>(message.reason));
    out->set_score(message.score);
    for (const std::uint32_t score : message.playerScores) {
        out->add_player_scores(score);
    }
}

void fill(pb::Envelope& envelope, const PlayerLeft& message) {
    envelope.mutable_player_left()->set_client_id(message.clientId);
}

void fill(pb::Envelope& envelope, const Leave& /*message*/) { envelope.mutable_leave(); }

void fill(pb::Envelope& envelope, const Chat& message) {
    auto* out = envelope.mutable_chat();
    out->set_client_id(message.clientId);
    out->set_text(message.text);
}

void fill(pb::Envelope& envelope, const PlayerInput& message) {
    auto* out = envelope.mutable_player_input();
    out->set_sequence(message.sequence);
    out->set_buttons(message.buttons);
}

void fill(pb::Envelope& envelope, const WorldState& message) {
    auto* out = envelope.mutable_world_state();
    out->set_tick(message.tick);
    out->set_last_processed_input(message.lastProcessedInput);
    out->mutable_entities()->Reserve(static_cast<int>(message.entities.size()));
    for (const auto& [entityId, type, subtype, x, y, health, flags] : message.entities) {
        auto* entity = out->add_entities();
        entity->set_entity_id(entityId);
        entity->set_type(static_cast<pb::EntityType>(type));
        entity->set_subtype(subtype);
        entity->set_x(x);
        entity->set_y(y);
        entity->set_health(health);
        entity->set_flags(flags);
    }
}

/** @brief Narrows an unsigned protobuf field, or fails if it does not fit (or exceeds `max`). */
template <typename T>
std::optional<T> narrow(const std::uint32_t value, const std::uint32_t max = std::numeric_limits<T>::max()) {
    if (value > max) {
        return std::nullopt;
    }
    return static_cast<T>(value);
}

/** @brief Narrows a signed protobuf field to `int16_t`, or fails if it does not fit. */
std::optional<std::int16_t> narrowI16(const std::int32_t value) {
    if (value < std::numeric_limits<std::int16_t>::min() || value > std::numeric_limits<std::int16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::int16_t>(value);
}

std::optional<Message> read(const pb::Welcome& in) {
    const auto port = narrow<std::uint16_t>(in.udp_port());
    if (!port || *port == 0) {
        return std::nullopt;
    }
    return Welcome{.udpPort = *port};
}

std::optional<Message> read(const pb::LobbyList& in) {
    if (std::cmp_greater(in.rooms_size(), MAX_ROOMS)) {
        return std::nullopt;
    }
    LobbyList out;
    out.rooms.reserve(static_cast<std::size_t>(in.rooms_size()));
    for (const auto& room : in.rooms()) {
        const auto playerCount = narrow<std::uint8_t>(room.player_count(), MAX_PLAYERS);
        const auto maxPlayers = narrow<std::uint8_t>(room.max_players(), MAX_PLAYERS);
        if (!playerCount || !maxPlayers || *maxPlayers == 0 || *playerCount > *maxPlayers ||
            !pb::RoomState_IsValid(room.state())) {
            return std::nullopt;
        }
        out.rooms.push_back({
            .roomId = room.room_id(),
            .playerCount = *playerCount,
            .maxPlayers = *maxPlayers,
            .state = static_cast<RoomState>(room.state()),
        });
    }
    return out;
}

std::optional<Message> read(const pb::LobbyUpdate& in) {
    if (std::cmp_greater(in.slots_size(), MAX_PLAYERS)) {
        return std::nullopt;
    }
    LobbyUpdate out{.roomId = in.room_id(), .slots = {}};
    out.slots.reserve(static_cast<std::size_t>(in.slots_size()));
    for (const auto& slot : in.slots()) {
        out.slots.push_back({.clientId = slot.client_id(), .ready = slot.ready()});
    }
    return out;
}

std::optional<Message> read(const pb::GameStart& in) {
    const auto playerCount = narrow<std::uint8_t>(in.player_count(), MAX_PLAYERS);
    if (!playerCount || *playerCount == 0 || in.player_index() >= *playerCount) {
        return std::nullopt;
    }
    return GameStart{
        .roomId = in.room_id(),
        .playerIndex = static_cast<std::uint8_t>(in.player_index()),
        .playerCount = *playerCount,
    };
}

std::optional<Message> read(const pb::GameOver& in) {
    if (!pb::GameOverReason_IsValid(in.reason()) || std::cmp_greater(in.player_scores_size(), MAX_PLAYERS)) {
        return std::nullopt;
    }
    return GameOver{
        .reason = static_cast<GameOverReason>(in.reason()),
        .score = in.score(),
        .playerScores = {in.player_scores().begin(), in.player_scores().end()},
    };
}

std::optional<Message> read(const pb::Chat& in) {
    if (in.text().empty() || in.text().size() > MAX_CHAT_LENGTH) {
        return std::nullopt;
    }
    return Chat{.clientId = in.client_id(), .text = in.text()};
}

std::optional<Message> read(const pb::PlayerInput& in) {
    return PlayerInput{.sequence = in.sequence(), .buttons = static_cast<std::uint8_t>(in.buttons() & BUTTONS_MASK)};
}

std::optional<Message> read(const pb::WorldState& in) {
    WorldState out{.tick = in.tick(), .lastProcessedInput = in.last_processed_input(), .entities = {}};
    out.entities.reserve(static_cast<std::size_t>(in.entities_size()));
    for (const auto& entity : in.entities()) {
        const auto subtype = narrow<std::uint8_t>(entity.subtype());
        const auto x = narrowI16(entity.x());
        const auto y = narrowI16(entity.y());
        const auto health = narrow<std::uint8_t>(entity.health());
        const auto flags = narrow<std::uint8_t>(entity.flags());
        if (!pb::EntityType_IsValid(entity.type()) || !subtype || !x || !y || !health || !flags) {
            return std::nullopt;
        }
        out.entities.push_back({
            .entityId = entity.entity_id(),
            .type = static_cast<EntityType>(entity.type()),
            .subtype = *subtype,
            .x = *x,
            .y = *y,
            .health = *health,
            .flags = *flags,
        });
    }
    return out;
}

}  // namespace

std::vector<std::uint8_t> encode(const Message& message) {
    pb::Envelope envelope;
    std::visit([&envelope](const auto& value) { fill(envelope, value); }, message);

    std::vector<std::uint8_t> bytes(envelope.ByteSizeLong());
    envelope.SerializeToArray(bytes.data(), static_cast<int>(bytes.size()));
    return bytes;
}

std::optional<Message> decode(const std::span<const std::uint8_t> bytes) {
    if (bytes.empty() || bytes.size() > MAX_MESSAGE_SIZE) {
        return std::nullopt;
    }
    pb::Envelope envelope;
    if (!envelope.ParseFromArray(bytes.data(), static_cast<int>(bytes.size()))) {
        return std::nullopt;
    }

    switch (envelope.message_case()) {
        case pb::Envelope::kHello:
            return Hello{.version = envelope.hello().version()};
        case pb::Envelope::kWelcome:
            return read(envelope.welcome());
        case pb::Envelope::kLobbyListRequest:
            return LobbyListRequest{};
        case pb::Envelope::kLobbyList:
            return read(envelope.lobby_list());
        case pb::Envelope::kLobbyCreate:
            return LobbyCreate{};
        case pb::Envelope::kLobbyJoin:
            return LobbyJoin{.roomId = envelope.lobby_join().room_id()};
        case pb::Envelope::kLobbyReady:
            return LobbyReady{.ready = envelope.lobby_ready().ready()};
        case pb::Envelope::kLobbyUpdate:
            return read(envelope.lobby_update());
        case pb::Envelope::kGameStart:
            return read(envelope.game_start());
        case pb::Envelope::kGameOver:
            return read(envelope.game_over());
        case pb::Envelope::kPlayerLeft:
            return PlayerLeft{.clientId = envelope.player_left().client_id()};
        case pb::Envelope::kLeave:
            return Leave{};
        case pb::Envelope::kChat:
            return read(envelope.chat());
        case pb::Envelope::kPlayerInput:
            return read(envelope.player_input());
        case pb::Envelope::kWorldState:
            return read(envelope.world_state());
        case pb::Envelope::MESSAGE_NOT_SET:
            break;
    }
    // Empty envelope, or a message added by a newer peer that this build does not know.
    return std::nullopt;
}

}  // namespace rtype::protocol
