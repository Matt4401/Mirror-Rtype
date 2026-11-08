#include "protocol/Protocol.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace rlss::protocol {

namespace {

constexpr unsigned BYTE_BITS = 8;
constexpr std::uint32_t BYTE_MASK = 0xFFU;
constexpr std::size_t MATCH_LIST_HEADER_SIZE = 4;
constexpr std::size_t MATCH_INFO_SIZE = 11;
constexpr std::size_t SNAPSHOT_HEADER_SIZE = 14;
constexpr std::size_t ENTITY_SIZE = 22;
constexpr std::size_t PLAYER_INPUT_SIZE = 8;
constexpr std::size_t MATCH_STATUS_SIZE = 10;
constexpr std::size_t WELCOME_SIZE = 6;
constexpr std::size_t JOIN_MATCH_SIZE = 3;
constexpr std::size_t JOIN_RESULT_SIZE = 4;
constexpr std::uint8_t FLAG_OVERTIME = 0x01U;
constexpr float STEER_SCALE = 127.0F;
constexpr std::uint8_t FLAG_FACING_LEFT = 0x01U;
constexpr std::uint8_t FLAG_BOOSTING = 0x02U;
constexpr std::uint8_t FLAG_FLIP_RESET = 0x04U;
constexpr std::uint8_t KNOWN_FLAGS = FLAG_FACING_LEFT | FLAG_BOOSTING | FLAG_FLIP_RESET;
constexpr std::uint8_t MAX_BOOST = 100;
constexpr unsigned NIBBLE_BITS = 4;
constexpr std::uint8_t NIBBLE_MASK = 0x0FU;
constexpr std::uint8_t BUTTON_JUMP = 0x01U;
constexpr std::uint8_t BUTTON_BOOST = 0x02U;
constexpr std::uint8_t KNOWN_BUTTONS = BUTTON_JUMP | BUTTON_BOOST;

/** @brief One byte holding `low` in its low 4 bits and `high` in its high 4 bits (both wrapped to 4 bits). */
std::uint8_t packNibbles(const std::uint8_t low, const std::uint8_t high) {
    return static_cast<std::uint8_t>((low & NIBBLE_MASK) | ((high & NIBBLE_MASK) << NIBBLE_BITS));
}

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

    /** @brief Reads `length` bytes as text; the caller checked `remaining()` first. */
    std::string text(const std::size_t length) {
        std::string value(length, ' ');
        for (char& character : value) {
            character = static_cast<char>(u8());
        }
        return value;
    }

  private:
    std::span<const std::uint8_t> _data;
};

/** @brief Reads `[length u8][name]`; nothing if it runs past the end or the name is not valid. */
std::optional<std::string> readName(Reader& reader) {
    if (reader.remaining() < 1) {
        return std::nullopt;
    }
    const std::size_t length = reader.u8();
    if (length == 0 || length > MAX_NAME_LENGTH || reader.remaining() < length) {
        return std::nullopt;
    }
    std::string name = reader.text(length);
    if (!isValidName(name)) {
        return std::nullopt;
    }
    return name;
}

/** @brief Writes `[length u8][name]`, the name cut to `MAX_NAME_LENGTH` characters. */
void writeName(Writer& writer, const std::string_view name) {
    const std::string_view kept = name.substr(0, MAX_NAME_LENGTH);
    writer.u8(static_cast<std::uint8_t>(kept.size()));
    for (const char character : kept) {
        writer.u8(static_cast<std::uint8_t>(character));
    }
}

std::optional<Message> decodeMatchList(Reader& reader) {
    if (reader.remaining() < MATCH_LIST_HEADER_SIZE - 1) {
        return std::nullopt;
    }
    MatchList list;
    list.online = reader.u16();
    const std::size_t count = reader.u8();
    if (count > MAX_MATCHES || reader.remaining() != count * MATCH_INFO_SIZE) {
        return std::nullopt;
    }
    list.matches.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        MatchInfo info;
        info.id = reader.u16();
        info.bluePlayers = reader.u8();
        info.orangePlayers = reader.u8();
        info.blueScore = reader.u16();
        info.orangeScore = reader.u16();
        info.clock = reader.u16();
        const std::uint8_t flags = reader.u8();
        if (info.id == 0 || info.bluePlayers > PLAYERS_PER_TEAM || info.orangePlayers > PLAYERS_PER_TEAM ||
            info.clock > MAX_CLOCK || (flags & ~FLAG_OVERTIME) != 0) {
            return std::nullopt;
        }
        info.overtime = (flags & FLAG_OVERTIME) != 0;
        list.matches.push_back(info);
    }
    return list;
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
    // Sound cues: counts then values, the first cue in the low 4 bits, the second in the high 4 bits (all valid).
    const std::uint8_t counts = reader.u8();
    const std::uint8_t values = reader.u8();
    entity.firstCue = {
        .count = static_cast<std::uint8_t>(counts & NIBBLE_MASK),
        .value = static_cast<std::uint8_t>(values & NIBBLE_MASK),
    };
    entity.secondCue = {
        .count = static_cast<std::uint8_t>(counts >> NIBBLE_BITS),
        .value = static_cast<std::uint8_t>(values >> NIBBLE_BITS),
    };
    if (entity.id == 0 || !x || !y || !rotation || (flags & ~KNOWN_FLAGS) != 0 || entity.boost > MAX_BOOST) {
        return std::nullopt;
    }
    entity.x = *x;
    entity.y = *y;
    entity.rotation = *rotation;
    entity.facingLeft = (flags & FLAG_FACING_LEFT) != 0;
    entity.boosting = (flags & FLAG_BOOSTING) != 0;
    entity.flipReset = (flags & FLAG_FLIP_RESET) != 0;
    return entity;
}

std::optional<Message> decodeSnapshot(Reader& reader) {
    if (reader.remaining() < SNAPSHOT_HEADER_SIZE - 1) {
        return std::nullopt;
    }
    Snapshot snapshot;
    snapshot.matchId = reader.u16();
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
    const auto pitch = static_cast<std::int8_t>(reader.u8());
    const std::uint8_t buttons = reader.u8();
    if (steer < -static_cast<int>(STEER_SCALE) || pitch < -static_cast<int>(STEER_SCALE) ||
        (buttons & ~KNOWN_BUTTONS) != 0) {
        return std::nullopt;
    }
    input.steer = static_cast<float>(steer) / STEER_SCALE;
    input.pitch = static_cast<float>(pitch) / STEER_SCALE;
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
    status.clock = reader.u16();
    const std::uint8_t flags = reader.u8();
    if (phase > static_cast<std::uint8_t>(MatchPhase::Ended) || status.countdown > MAX_COUNTDOWN ||
        status.clock > MAX_CLOCK || (flags & ~FLAG_OVERTIME) != 0) {
        return std::nullopt;
    }
    status.phase = static_cast<MatchPhase>(phase);
    status.overtime = (flags & FLAG_OVERTIME) != 0;
    return status;
}

std::optional<Message> decodeWelcome(Reader& reader) {
    if (reader.remaining() != WELCOME_SIZE - 1) {
        return std::nullopt;
    }
    const std::uint32_t carId = reader.u32();
    const std::uint8_t team = reader.u8();
    if (carId == 0 || team > static_cast<std::uint8_t>(Team::Orange)) {
        return std::nullopt;
    }
    return Welcome{.carId = carId, .team = static_cast<Team>(team)};
}

/** @brief Messages made of their type alone. */
template <typename Empty>
std::optional<Message> decodeEmpty(const Reader& reader) {
    if (reader.remaining() != 0) {
        return std::nullopt;
    }
    return Empty{};
}

std::optional<Message> decodeJoinMatch(Reader& reader) {
    if (reader.remaining() != JOIN_MATCH_SIZE - 1) {
        return std::nullopt;
    }
    const JoinMatch join{.matchId = reader.u16()};
    if (join.matchId == 0) {
        return std::nullopt;
    }
    return join;
}

std::optional<Message> decodeJoinResult(Reader& reader) {
    if (reader.remaining() != JOIN_RESULT_SIZE - 1) {
        return std::nullopt;
    }
    const std::uint8_t status = reader.u8();
    const std::uint16_t matchId = reader.u16();
    if (status > static_cast<std::uint8_t>(JoinStatus::ServerFull) ||
        ((status == static_cast<std::uint8_t>(JoinStatus::Joined)) != (matchId != 0))) {
        return std::nullopt;
    }
    return JoinResult{.status = static_cast<JoinStatus>(status), .matchId = matchId};
}

std::optional<Message> decodeSetName(Reader& reader) {
    auto name = readName(reader);
    if (!name || reader.remaining() != 0) {
        return std::nullopt;
    }
    return SetName{.name = std::move(*name)};
}

std::optional<Message> decodePlayerNames(Reader& reader) {
    if (reader.remaining() < 1) {
        return std::nullopt;
    }
    const std::size_t count = reader.u8();
    if (count > MAX_PLAYERS_PER_MATCH) {
        return std::nullopt;
    }
    PlayerNames names;
    names.players.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        if (reader.remaining() < sizeof(std::uint32_t)) {
            return std::nullopt;
        }
        const std::uint32_t carId = reader.u32();
        auto name = readName(reader);
        if (carId == 0 || !name) {
            return std::nullopt;
        }
        names.players.push_back(PlayerName{.carId = carId, .name = std::move(*name)});
    }
    if (reader.remaining() != 0) {
        return std::nullopt;
    }
    return names;
}

/** @brief A message made of its type alone. */
Bytes encodeType(const MessageType type) { return Bytes{static_cast<std::uint8_t>(type)}; }

}  // namespace

Bytes encode(const MatchList& message) {
    const std::size_t count = std::min(message.matches.size(), static_cast<std::size_t>(MAX_MATCHES));
    Bytes out;
    out.reserve(MATCH_LIST_HEADER_SIZE + (count * MATCH_INFO_SIZE));
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::MatchList));
    writer.u16(message.online);
    writer.u8(static_cast<std::uint8_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const MatchInfo& info = message.matches.at(i);
        writer.u16(info.id);
        writer.u8(std::min(info.bluePlayers, PLAYERS_PER_TEAM));
        writer.u8(std::min(info.orangePlayers, PLAYERS_PER_TEAM));
        writer.u16(info.blueScore);
        writer.u16(info.orangeScore);
        writer.u16(std::min(info.clock, MAX_CLOCK));
        writer.u8(info.overtime ? FLAG_OVERTIME : 0);
    }
    return out;
}

Bytes encode(const Snapshot& message) {
    const std::size_t count = std::min(message.entities.size(), MAX_ENTITIES_PER_PART);
    Bytes out;
    out.reserve(SNAPSHOT_HEADER_SIZE + (count * ENTITY_SIZE));
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::Snapshot));
    writer.u16(message.matchId);
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
        if (entity.flipReset) {
            flags |= FLAG_FLIP_RESET;
        }
        writer.u8(flags);
        writer.u8(entity.color);
        writer.u8(std::min(entity.boost, MAX_BOOST));
        writer.u8(packNibbles(entity.firstCue.count, entity.secondCue.count));
        writer.u8(packNibbles(entity.firstCue.value, entity.secondCue.value));
    }
    return out;
}

Bytes encode(const PlayerInput& message) {
    Bytes out;
    out.reserve(PLAYER_INPUT_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::PlayerInput));
    writer.u32(message.sequence);
    for (const float axis : {message.steer, message.pitch}) {
        const float value = std::isfinite(axis) ? std::clamp(axis, -1.0F, 1.0F) : 0.0F;
        writer.u8(static_cast<std::uint8_t>(static_cast<std::int8_t>(std::lround(value * STEER_SCALE))));
    }
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
    writer.u16(std::min(message.clock, MAX_CLOCK));
    writer.u8(message.overtime ? FLAG_OVERTIME : 0);
    return out;
}

Bytes encode(const Welcome& message) {
    Bytes out;
    out.reserve(WELCOME_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::Welcome));
    writer.u32(message.carId);
    writer.u8(static_cast<std::uint8_t>(message.team));
    return out;
}

Bytes encode(const CreateMatch& /*message*/) { return encodeType(MessageType::CreateMatch); }

Bytes encode(const JoinMatch& message) {
    Bytes out;
    out.reserve(JOIN_MATCH_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::JoinMatch));
    writer.u16(message.matchId);
    return out;
}

Bytes encode(const LeaveMatch& /*message*/) { return encodeType(MessageType::LeaveMatch); }

Bytes encode(const JoinResult& message) {
    Bytes out;
    out.reserve(JOIN_RESULT_SIZE);
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::JoinResult));
    writer.u8(static_cast<std::uint8_t>(message.status));
    writer.u16(message.status == JoinStatus::Joined ? message.matchId : 0);
    return out;
}

Bytes encode(const SetName& message) {
    Bytes out;
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::SetName));
    writeName(writer, message.name);
    return out;
}

Bytes encode(const PlayerNames& message) {
    const std::size_t count = std::min(message.players.size(), static_cast<std::size_t>(MAX_PLAYERS_PER_MATCH));
    Bytes out;
    Writer writer(out);
    writer.u8(static_cast<std::uint8_t>(MessageType::PlayerNames));
    writer.u8(static_cast<std::uint8_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const PlayerName& player = message.players.at(i);
        writer.u32(player.carId);
        writeName(writer, player.name);
    }
    return out;
}

bool isNameCharacter(const char character) noexcept {
    return (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9') || character == ' ' ||
           character == '-' || character == '.';
}

bool isValidName(const std::string_view name) noexcept {
    return !name.empty() && name.size() <= MAX_NAME_LENGTH && name.front() != ' ' && name.back() != ' ' &&
           std::ranges::all_of(name, isNameCharacter);
}

std::optional<Message> decode(const std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return std::nullopt;
    }
    Reader reader(data);
    switch (static_cast<MessageType>(reader.u8())) {
        case MessageType::MatchList:
            return decodeMatchList(reader);
        case MessageType::Snapshot:
            return decodeSnapshot(reader);
        case MessageType::PlayerInput:
            return decodePlayerInput(reader);
        case MessageType::MatchStatus:
            return decodeMatchStatus(reader);
        case MessageType::Welcome:
            return decodeWelcome(reader);
        case MessageType::CreateMatch:
            return decodeEmpty<CreateMatch>(reader);
        case MessageType::JoinMatch:
            return decodeJoinMatch(reader);
        case MessageType::LeaveMatch:
            return decodeEmpty<LeaveMatch>(reader);
        case MessageType::JoinResult:
            return decodeJoinResult(reader);
        case MessageType::SetName:
            return decodeSetName(reader);
        case MessageType::PlayerNames:
            return decodePlayerNames(reader);
    }
    return std::nullopt;
}

std::optional<std::uint16_t> snapshotMatch(const std::span<const std::uint8_t> data) {
    if (data.size() < SNAPSHOT_HEADER_SIZE || data.front() != static_cast<std::uint8_t>(MessageType::Snapshot)) {
        return std::nullopt;
    }
    Reader reader(data.subspan(1));
    return reader.u16();
}

}  // namespace rlss::protocol
