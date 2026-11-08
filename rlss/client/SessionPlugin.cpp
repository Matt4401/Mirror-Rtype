#include "SessionPlugin.hpp"

#include <memory>
#include <utility>

#include "ServerLink.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "log/Logger.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"

namespace rlss::client {

namespace {

constexpr const char* LOG_TAG = "client";

}  // namespace

SessionPlugin::SessionPlugin(std::shared_ptr<ecs::net::INetworkClient> transport) : _transport(std::move(transport)) {}

void SessionPlugin::build(engine::PluginBuilder& builder) { builder.require<ecs::net::NetworkPlugin>(); }

void SessionPlugin::init(engine::Engine& engine) {
    auto& events = engine.eventBus();
    _disconnected =
        events.subscribe<ecs::net::PeerDisconnected>([this, &engine](const ecs::net::PeerDisconnected& /*event*/) {
            engine::log::error(LOG_TAG, "disconnected: ", describe(_transport->disconnectReason()));
            _lost = true;
            engine.stop();
        });
}

void SessionPlugin::shutdown(engine::Engine& engine) {
    auto& events = engine.eventBus();
    if (_disconnected) {
        events.unsubscribe<ecs::net::PeerDisconnected>(*_disconnected);
        _disconnected.reset();
    }
}

}  // namespace rlss::client
