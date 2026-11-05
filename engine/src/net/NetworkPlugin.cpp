
#include "NetworkPlugin.hpp"

#include <memory>
#include <utility>

#include "INetworkEventSource.hpp"
#include "INetworkManager.hpp"
#include "INetworkService.hpp"
#include "NetworkEntityMap.hpp"
#include "NetworkEvents.hpp"
#include "core/Engine.hpp"
#include "core/IPlugin.hpp"
#include "core/MediatorError.hpp"
#include "core/PluginBuilder.hpp"
#include "core/plugins/CorePlugin.hpp"
#include "ecs/Entity.hpp"

namespace ecs::net {

NetworkPlugin::NetworkPlugin(std::shared_ptr<INetworkEventSource> source, std::shared_ptr<INetworkService> service)
    : _source(std::move(source)), _service(std::move(service)) {}

void NetworkPlugin::build(engine::PluginBuilder& builder) { builder.require<engine::CorePlugin>(); }

void NetworkPlugin::init(engine::Engine& engine) {
    if (const auto res = engine.mediator().registerService<INetworkService>(_service); !res) {
        throw engine::MediatorError(res.error());
    }
    // Reuse a map created by the game before init: replacing it would drop its bindings and dangle references to it.
    auto& registry = engine.registry();
    auto& map = registry.hasResource<NetworkEntityMap>() ? registry.getResource<NetworkEntityMap>()
                                                         : registry.emplaceResource<NetworkEntityMap>();
    _destroyListener = engine.registry().addDestroyListener([map = &map](const Entity e) { map->unbindEntity(e); });
}

void NetworkPlugin::preUpdate(engine::Engine& engine) {
    for (auto& [type, peer, data, channel] : _source->poll()) {
        switch (type) {
            case EventType::Connected:
                engine.eventBus().publish(PeerConnected{peer});
                break;
            case EventType::Disconnected:
                engine.eventBus().publish(PeerDisconnected{peer});
                break;
            case EventType::Data:
                engine.eventBus().publish(PacketReceived{.peer = peer, .channel = channel, .data = std::move(data)});
                break;
        }
    }
}

void NetworkPlugin::shutdown(engine::Engine& engine) {
    if (_destroyListener) {
        engine.registry().removeDestroyListener(*_destroyListener);
        _destroyListener.reset();
    }
    if (engine.mediator().hasService<INetworkService>()) {
        engine.mediator().removeService<INetworkService>();
    }
}
}  // namespace ecs::net
