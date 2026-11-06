#include "protocol/Protocol.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace rlss::protocol {

namespace {

constexpr std::size_t LOBBY_STATE_SIZE = 3;
constexpr unsigned BYTE_BITS = 8;
constexpr std::uint16_t BYTE_MASK = 0xFFU;

}  // namespace

Bytes encode(const LobbyState& message) {
    return {
        static_cast<std::uint8_t>(MessageType::LobbyState),
        static_cast<std::uint8_t>((message.players >> BYTE_BITS) & BYTE_MASK),
        static_cast<std::uint8_t>(message.players & BYTE_MASK),
    };
}

std::optional<Message> decode(const std::span<const std::uint8_t> data) {
    if (data.empty()) {
        return std::nullopt;
    }
    switch (static_cast<MessageType>(data.front())) {
        case MessageType::LobbyState: {
            if (data.size() != LOBBY_STATE_SIZE) {
                return std::nullopt;
            }
            const std::uint8_t high = data.subspan(1).front();
            const std::uint8_t low = data.back();
            const auto players = static_cast<std::uint16_t>((high << BYTE_BITS) | low);
            return LobbyState{.players = players};
        }
    }
    return std::nullopt;
}

}  // namespace rlss::protocol
