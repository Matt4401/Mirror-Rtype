#include "protocol/Protocol.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rlss::protocol {

namespace {

constexpr unsigned BYTE_BITS = 8;
constexpr std::uint32_t BYTE_MASK = 0xFFU;
constexpr std::size_t LOBBY_STATE_SIZE = 3;
constexpr std::size_t SNAPSHOT_HEADER_SIZE = 12;
constexpr std::size_t ENTITY_SIZE = 20;
constexpr std::size_t PLAYER_INPUT_SIZE = 7;
constexpr std::size_t MATCH_STATUS_SIZE = 7;
constexpr std::size_t WELCOME_SIZE = 5;
constexpr float STEER_SCALE = 127.0F;
constexpr std::uint8_t FLAG_FACING_LEFT = 0x01U;
constexpr std::uint8_t FLAG_BOOSTING = 0x02U;
constexpr std::uint8_t KNOWN_FLAGS = FLAG_FACING_LEFT | FLAG_BOOSTING;
constexpr std::uint8_t MAX_BOOST = 100;
constexpr std::uint8_t BUTTON_JUMP = 0x01U;
constexpr std::uint8_t BUTTON_BOOST = 0x02U;
constexpr std::uint8_t KNOWN_BUTTONS = BUTTON_JUMP | BUTTON_BOOST;

/** @brief Appends big-endian values to a byte buffer. */
class Writer {
  public:
    explicit Writer(Bytes& out) : _out(&out) {}

    void u8(const std::uint8_t value) { _out->push_back(value); }

    void u16(const std::uint16_t value) {
        u8(static_cast<std::uint8_t>((value >> BYTE_BITS) & BYTE_MASK));
        u8(static_cast<std::uint8_t>(value & BYTE_MASK));
    }

    void u32(const std::uint32_t value) {
        for (unsigned shift = 3 * BYTE_BITS;; shift -= BYTE_BITS) {
            u8(static_cast<std::uint8_t>((value >> shift) & BYTE_MASK));
            if (shift == 0) {
                break;
            }
        }
    }

    void f32(const float value) { u32(std::bit_cast<std::uint32_t>(value)); }

  private:
    Bytes* _out;
};

/** @brief Reads big-endian values from an untrusted buffer; every read is bounds-checked by `size()` first. */
class Reader {
  public:
    explicit Reader(const std::span<const std::uint8_t> data) : _data(data) {}

    [[nodiscard]] std::size_t remaining() const noexcept { return _data.size(); }

    std::uint8_t u8() {
        const std::uint8_t value = _data.front();
        _data = _data.subspan(1);
        return value;
    }

    std::uint16_t u16() {
        const auto high = static_cast<std::uint16_t>(u8());
        return static_cast<std::uint16_t>((high << BYTE_BITS) | u8());
    }

    std::uint32_t u32() {
        std::uint32_t value = 0;
        for (int i = 0; i < 4; ++i) {
            value = (value << BYTE_BITS) | u8();
        }
        return value;
    }

    std::optional<float> f32() {
        const auto value = std::bit_cast<float>(u32());
        if (!std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    }

  private:
    std::span<const std::uint8_t> _data;
};

std::optional<Message> decodeLobbyState(Reader& reader) {
    if (reader.remaining() != LOBBY_STATE_SIZE - 1) {
        return std::nullopt;
    }
    return LobbyState{.players = reader.u16()};
}

std::optional<EntityState> decodeEntity(Reader& reader) {
    EntityState entity;
    entity.id = reader.u32();
    entity.kind = reader.u8();
    const auto x = reader.f32();
    const auto y = reader.f32();
    const auto rotation = reader.f32();
    const std::uint8_t flags = reader.u8();
    entity.color = reader.u8();
    entity.boost = reader.u8();
    if (entity.id == 0 || !x || !y || !rotation || (flags & ~KNOWN_FLAGS) != 0 || entity.boost > MAX_BOOST) {
        return std::nullopt;
    }
    entity.x = *x;
    entity.y = *y;
    entity.rotation = *rotation;
    entity.facingLeft = (flags & FLAG_FACING_LEFT) != 0;
    entity.boosting = (flags & FLAG_BOOSTING) != 0;
    return entity;
}

std::optional<Message> decodeSnapshot(Reader& reader) {
    if (reader.remaining() < SNAPSHOT_HEADER_SIZE - 1) {
        return std::nullopt;
    }
    Snapshot snapshot;
    snapshot.tick = reader.u32();
    snapshot.lastProcessedInput = reader.u32();
    snapshot.partIndex = reader.u8();
    snapshot.partCount = reader.u8();
    const std::size_t count = reader.u8();
    if (snapshot.partCount == 0 || snapshot.partCount > MAX_SNAPSHOT_PARTS ||
        snapshot.partIndex >= snapshot.partCount || count > MAX_ENTITIES_PER_PART ||
        reader.remaining() != count * ENTITY_SIZE) {
        return std::nullopt;
    }
    snapshot.entities.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto entity = decodeEntity(reader);
        if (!entity) {
            return std::nullopt;
        }
        snapshot.entities.push_back(*entity);
    }
    return snapshot;
}

std::optional<Message> decodePlayerInput(Reader& reader) {
    if (reader.remaining() != PLAYER_INPUT_SIZE - 1) {
        return std::nullopt;
    }
    PlayerInput input;
    input.sequence = reader.u32();
    const auto steer = static_cast<std::int8_t>(reader.u8());
    const std::uint8_t buttons = reader.u8();
    if (steer < -static_cast<int>(STEER_SCALE) || (buttons & ~KNOWN_BUTTONS) != 0) {
        return std::nullopt;
    }
    input.steer = static_cast<float>(steer) / STEER_SCALE;
    input.jump = (buttons & BUTTON_JUMP) != 0;
    input.boost = (buttons & BUTTON_BOOST) != 0;
    return input;
}

std::optional<Message> decodeMatchStatus(Reader& reader) {
    if (reader.remaining() != MATCH_STATUS_SIZE - 1) {
        return std::nullopt;
    }
    MatchStatus status;
    status.blueScore = reader.u16();
    status.orangeScore = reader.u16();
    const std::uint8_t phase = reader.u8();
    status.countdown = reader.u8();
    if (phase > static_cast<std::uint8_t>(MatchPhase::Kickoff) || status.countdown > MAX_COUNTDOWN) {
        return std::nullopt;
    }
    status.phase = static_cast<MatchPhase>(phase);
    return status;
}

std::optional<Message> decodeWelcome(Reader& reader) {
    if (reader.remaining() != WELCOME_SIZE - 1) {
        return std::nullopt;
    }
    const Welcome welcome{.carId = reader.u32()};
    if (welcome.carId == 0) {
        return std::nullopt;
    }
    return welcome;
}

}  // namespace

Bytes encode(const LobbyState& message) {
    Bytes out;
    out.reserve(LOBBY_STATE_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::LobbyState));
    writer.u16(message.players);
    return out;
}

Bytes encode(const Snapshot& message) {
    const std::size_t count = std::min(message.entities.size(), MAX_ENTITIES_PER_PART);
    Bytes out;
    out.reserve(SNAPSHOT_HEADER_SIZE + (count * ENTITY_SIZE));
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::Snapshot));
    writer.u32(message.tick);
    writer.u32(message.lastProcessedInput);
    writer.u8(message.partIndex);
    writer.u8(message.partCount);
    writer.u8(static_cast<std::uint8_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const EntityState& entity = message.entities.at(i);
        writer.u32(entity.id);
        writer.u8(entity.kind);
        writer.f32(entity.x);
        writer.f32(entity.y);
        writer.f32(entity.rotation);
        std::uint8_t flags = entity.facingLeft ? FLAG_FACING_LEFT : 0;
        if (entity.boosting) {
            flags |= FLAG_BOOSTING;
        }
        writer.u8(flags);
        writer.u8(entity.color);
        writer.u8(std::min(entity.boost, MAX_BOOST));
    }
    return out;
}

Bytes encode(const PlayerInput& message) {
    Bytes out;
    out.reserve(PLAYER_INPUT_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::PlayerInput));
    writer.u32(message.sequence);
    const float steer = std::isfinite(message.steer) ? std::clamp(message.steer, -1.0F, 1.0F) : 0.0F;
    writer.u8(static_cast<std::uint8_t>(static_cast<std::int8_t>(std::lround(steer * STEER_SCALE))));
    std::uint8_t buttons = message.jump ? BUTTON_JUMP : 0;
    if (message.boost) {
        buttons |= BUTTON_BOOST;
    }
    writer.u8(buttons);
    return out;
}

Bytes encode(const MatchStatus& message) {
    Bytes out;
    out.reserve(MATCH_STATUS_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::MatchStatus));
    writer.u16(message.blueScore);
    writer.u16(message.orangeScore);
    writer.u8(static_cast<std::uint8_t>(message.phase));
    writer.u8(std::min(message.countdown, MAX_COUNTDOWN));
    return out;
}

Bytes encode(const Welcome& message) {
    Bytes out;
    out.reserve(WELCOME_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::Welcome));
    writer.u32(message.carId);
    return out;
}

std::optional<Message> decode(const std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return std::nullopt;
    }
    Reader reader(data);
    switch (static_cast<MessageType>(reader.u8())) {
        case MessageType::LobbyState:
            return decodeLobbyState(reader);
        case MessageType::Snapshot:
            return decodeSnapshot(reader);
        case MessageType::PlayerInput:
            return decodePlayerInput(reader);
        case MessageType::MatchStatus:
            return decodeMatchStatus(reader);
        case MessageType::Welcome:
            return decodeWelcome(reader);
    }
    return std::nullopt;
}

}  // namespace rlss::protocol
