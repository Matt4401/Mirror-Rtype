#include "MatchServer.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "Logger.hpp"
#include "MatchRoom.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkServer.hpp"
#include "protocol/Protocol.hpp"

namespace rlss::server {

namespace {

constexpr const char* LOG_TAG = "server";

/** @brief True for the messages the menu handles itself; the others go to the player's match. */
bool isMenuRequest(const ecs::net::Bytes& data) {
    if (data.empty()) {
        return false;
    }
    const auto type = static_cast<protocol::MessageType>(data.front());
    return type == protocol::MessageType::CreateMatch || type == protocol::MessageType::JoinMatch ||
           type == protocol::MessageType::LeaveMatch;
}

}  // namespace

MatchServer::MatchServer(std::shared_ptr<ecs::net::INetworkServer> transport, const bool threaded)
    : _transport(std::move(transport)), _threaded(threaded) {}

void MatchServer::update() {
    for (const ecs::net::NetworkEvent& event : _transport->poll()) {
        switch (event.type) {
            case ecs::net::EventType::Connected:
                onConnected(event.peer);
                break;
            case ecs::net::EventType::Disconnected:
                onDisconnected(event.peer);
                break;
            case ecs::net::EventType::Data:
                onData(event);
                break;
        }
    }
    if (!_threaded) {
        for (const auto& [id, room] : _rooms) {
            room->advance();
        }
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - _lastList >= LIST_INTERVAL) {
        _lastList = now;
        broadcastList();
    }
}

std::optional<std::uint16_t> MatchServer::matchOf(const ecs::net::PeerId peer) const {
    const auto it = _matchOf.find(peer);
    if (it == _matchOf.end()) {
        return std::nullopt;
    }
    return it->second;
}

protocol::MatchList MatchServer::matchList() const {
    protocol::MatchList list;
    list.online =
        static_cast<std::uint16_t>(std::min<std::size_t>(_online.size(), std::numeric_limits<std::uint16_t>::max()));
    for (const auto& [id, room] : _rooms) {
        const MatchSummary summary = room->summary();
        if (summary.status.phase == protocol::MatchPhase::Ended) {
            continue;
        }
        list.matches.push_back(protocol::MatchInfo{
            .id = id,
            .bluePlayers = summary.bluePlayers,
            .orangePlayers = summary.orangePlayers,
            .blueScore = summary.status.blueScore,
            .orangeScore = summary.status.orangeScore,
            .clock = summary.status.clock,
            .overtime = summary.status.overtime,
        });
    }
    return list;
}

void MatchServer::onConnected(const ecs::net::PeerId peer) {
    _online.insert(peer);
    rtype::log::info(LOG_TAG, "player ", peer, " connected (", _online.size(), " online)");
    sendList(peer);
}

void MatchServer::onDisconnected(const ecs::net::PeerId peer) {
    leave(peer);
    _online.erase(peer);
    rtype::log::info(LOG_TAG, "player ", peer, " disconnected (", _online.size(), " online)");
}

void MatchServer::onData(const ecs::net::NetworkEvent& event) {
    if (!_online.contains(event.peer)) {
        return;
    }
    if (!isMenuRequest(event.data)) {
        // Inputs: only a player in a match has a use for them.
        const auto match = matchOf(event.peer);
        if (match) {
            _rooms.at(*match)->deliver(event.peer, event.data, event.channel);
        }
        return;
    }
    const auto message = protocol::decode(event.data);
    if (!message) {
        return;  // untrusted bytes: a malformed request is ignored
    }
    if (std::holds_alternative<protocol::CreateMatch>(*message)) {
        create(event.peer);
    } else if (const auto* request = std::get_if<protocol::JoinMatch>(&*message)) {
        join(event.peer, request->matchId);
    } else if (std::holds_alternative<protocol::LeaveMatch>(*message)) {
        leave(event.peer);
        sendList(event.peer);
    }
}

void MatchServer::create(const ecs::net::PeerId peer) {
    if (_matchOf.contains(peer)) {
        return;  // already playing: the client is out of sync, its request is meaningless
    }
    if (_rooms.size() >= protocol::MAX_MATCHES) {
        sendResult(peer, protocol::JoinStatus::ServerFull);
        return;
    }
    const std::uint16_t id = nextMatchId();
    std::unique_ptr<MatchRoom> room;
    try {
        room = std::make_unique<MatchRoom>(*_transport, id);
    } catch (const std::exception& error) {
        rtype::log::error(LOG_TAG, "cannot start a match: ", error.what());
        sendResult(peer, protocol::JoinStatus::ServerFull);
        return;
    }
    if (_threaded) {
        room->runInBackground();
    }
    _rooms.emplace(id, std::move(room));
    rtype::log::info(LOG_TAG, "player ", peer, " created match ", id, " (", _rooms.size(), " matches)");
    join(peer, id);
}

void MatchServer::join(const ecs::net::PeerId peer, const std::uint16_t matchId) {
    if (_matchOf.contains(peer)) {
        return;
    }
    const auto it = _rooms.find(matchId);
    if (it == _rooms.end() || it->second->hasEnded()) {
        sendResult(peer, protocol::JoinStatus::NotFound);
        return;
    }
    if (it->second->isFull()) {
        sendResult(peer, protocol::JoinStatus::Full);
        return;
    }
    // The answer goes first: on the reliable channel, it reaches the client before what the match sends it.
    sendResult(peer, protocol::JoinStatus::Joined, matchId);
    it->second->addPlayer(peer);
    _matchOf.emplace(peer, matchId);
}

void MatchServer::leave(const ecs::net::PeerId peer) {
    const auto match = matchOf(peer);
    if (!match) {
        return;
    }
    _matchOf.erase(peer);
    const auto it = _rooms.find(*match);
    it->second->removePlayer(peer);
    if (it->second->playerCount() == 0) {
        _rooms.erase(it);
        rtype::log::info(LOG_TAG, "match ", *match, " closed (", _rooms.size(), " matches)");
    }
}

void MatchServer::sendResult(const ecs::net::PeerId peer, const protocol::JoinStatus status,
                             const std::uint16_t matchId) {
    _transport->send(peer, protocol::encode(protocol::JoinResult{.status = status, .matchId = matchId}),
                     ecs::net::Channel::Reliable);
}

void MatchServer::sendList(const ecs::net::PeerId peer) {
    _transport->send(peer, protocol::encode(matchList()), ecs::net::Channel::Reliable);
}

void MatchServer::broadcastList() {
    protocol::MatchList list = matchList();
    if (_lastSent && *_lastSent == list) {
        return;
    }
    const ecs::net::Bytes bytes = protocol::encode(list);
    for (const ecs::net::PeerId peer : _online) {
        if (!_matchOf.contains(peer)) {
            _transport->send(peer, bytes, ecs::net::Channel::Reliable);
        }
    }
    _lastSent = std::move(list);
}

std::uint16_t MatchServer::nextMatchId() {
    // At most MAX_MATCHES ids are in use: a free one is always found within a few tries.
    while (_nextId == 0 || _rooms.contains(_nextId)) {
        ++_nextId;
    }
    return _nextId++;
}

}  // namespace rlss::server
