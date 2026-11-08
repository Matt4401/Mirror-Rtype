#include "menu/MenuScenesPlugin.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <variant>

#include "AssetsPlugin.hpp"
#include "CameraPlugin.hpp"
#include "ClientEvents.hpp"
#include "audio/types/AudioTypes.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/IAudioService.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "menu/MenuScenes.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkManager.hpp"
#include "net/INetworkService.hpp"
#include "net/NetworkEvents.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "settings/SettingsEvents.hpp"
#include "settings/SettingsPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace rlss::client {

namespace {

/** @brief Saved player name. */
constexpr std::string_view NAME_KEY = "player.name";

/**
 * @brief Why a request was refused.
 * @param status Answer of the server.
 * @return Message for the player.
 */
std::string_view refusal(const protocol::JoinStatus status) {
    switch (status) {
        case protocol::JoinStatus::Full:
            return "PARTIE PLEINE";
        case protocol::JoinStatus::NotFound:
            return "PARTIE TERMINEE";
        case protocol::JoinStatus::ServerFull:
            return "SERVEUR PLEIN";
        case protocol::JoinStatus::Joined:
            break;
    }
    return {};
}

/**
 * @brief Sends a message to the server, on the reliable channel.
 * @param engine Host engine.
 * @param bytes Encoded message.
 */
void send(engine::Engine& engine, const protocol::Bytes& bytes) {
    engine.service<ecs::net::INetworkService>().send(ecs::net::SERVER_PEER, bytes, ecs::net::Channel::Reliable);
}

/**
 * @brief Gives the volume settings to the audio service, if there is one.
 * @param engine Host engine.
 * @param settings Settings.
 */
void applyVolumes(engine::Engine& engine, const config::Settings& settings) {
    auto audio = engine.mediator().getService<engine::IAudioService>();
    if (!audio) {
        return;
    }
    audio->get().setVolume(audio::AudioBus::Master, static_cast<float>(settings.getNumber("audio.master")));
    audio->get().setVolume(audio::AudioBus::Music, static_cast<float>(settings.getNumber("audio.music")));
    audio->get().setVolume(audio::AudioBus::Sfx, static_cast<float>(settings.getNumber("audio.sfx")));
}

}  // namespace

void MenuScenesPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ecs::net::NetworkPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
    builder.require<scene::ScenePlugin>();
    builder.require<settings::SettingsPlugin>();
    builder.require<ui::UiPlugin>();
    builder.require<CameraPlugin>();
    builder.require<AssetsPlugin>();
}

void MenuScenesPlugin::init(engine::Engine& engine) {
    ecs::Registry& registry = engine.registry();
    registry.emplaceResource<MenuState>();

    config::Settings& settings = engine.getPlugin<settings::SettingsPlugin>().settings();
    settings.define(std::string(NAME_KEY), std::string{});
    settings.define("audio.master", 1.0);
    settings.define("audio.music", 0.8);
    settings.define("audio.sfx", 1.0);
    applyVolumes(engine, settings);
    engine.eventBus().subscribe<settings::SettingChanged>([&engine, &settings](const settings::SettingChanged& event) {
        if (event.key.starts_with("audio.")) {
            applyVolumes(engine, settings);
        }
    });

    _packets =
        engine.eventBus().subscribe<ecs::net::PacketReceived>([this, &engine](const ecs::net::PacketReceived& packet) {
            if (const auto message = protocol::decode(packet.data)) {
                onMessage(engine, *message);
            }
        });

    scene::SceneManager& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
    scenes.registerScene<NameScene>();
    scenes.registerScene<MenuScene>();
    scenes.registerScene<BrowserScene>();
    scenes.registerScene<JoiningScene>();
    scenes.registerScene<MatchScene>();
    scenes.registerScene<EndedScene>();
    scenes.registerScene<SettingsScene>();

    // A name saved by an earlier session skips the name screen.
    if (!confirmName(engine, settings.getString(NAME_KEY))) {
        registry.getResource<MenuState>().notice.clear();
        scenes.switchTo<NameScene>();
    }
}

bool MenuScenesPlugin::confirmName(engine::Engine& engine, std::string typed) {
    while (!typed.empty() && typed.back() == ' ') {
        typed.pop_back();
    }
    auto& state = engine.registry().getResource<MenuState>();
    if (!protocol::isValidName(typed)) {
        state.notify("ENTRE UN PSEUDO");
        return false;
    }
    state.name = typed;
    send(engine, protocol::encode(protocol::SetName{.name = typed}));
    (void)engine.getPlugin<settings::SettingsPlugin>().settings().set(NAME_KEY, typed);
    engine.getPlugin<scene::ScenePlugin>().manager().switchTo<MenuScene>();
    return true;
}

void MenuScenesPlugin::request(engine::Engine& engine, const protocol::Bytes& request) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.requestPending) {
        return;  // a release and a confirm in the same frame, for instance: one request is enough
    }
    state.requestPending = true;
    send(engine, request);
    engine.getPlugin<scene::ScenePlugin>().manager().push<JoiningScene>();
}

void MenuScenesPlugin::cancelRequest(engine::Engine& engine) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.requestPending) {
        state.requestPending = false;
        ++state.cancelledRequests;
    }
}

void MenuScenesPlugin::leaveMatch(engine::Engine& engine) {
    auto& state = engine.registry().getResource<MenuState>();
    state.inMatch = false;
    state.endedShown = false;
    send(engine, protocol::encode(protocol::LeaveMatch{}));
    _link->setMatch(std::nullopt);
    // Every entity of the match goes (the views remove their parts on `ReplicatedEntityDespawned`), and its last tick
    // is forgotten: the next match counts its steps from 0.
    engine.getPlugin<ecs::net::ClientReplicationPlugin>().reset(engine);
    engine.eventBus().publish(MatchLeft{});
    engine.getPlugin<scene::ScenePlugin>().manager().switchTo<MenuScene>();
}

void MenuScenesPlugin::onMessage(engine::Engine& engine, const protocol::Message& message) {
    auto& state = engine.registry().getResource<MenuState>();
    // `state.inMatch` rather than the scene stack: `Welcome` or the final status can come in the same poll as the
    // `JoinResult`, before the queued switch to `MatchScene` is applied.
    if (const auto* list = std::get_if<protocol::MatchList>(&message)) {
        if (*list != state.matches) {
            state.matches = *list;
            engine.eventBus().publish(MatchListUpdated{});
        }
    } else if (const auto* result = std::get_if<protocol::JoinResult>(&message)) {
        onJoinResult(engine, *result);
    } else if (const auto* welcome = std::get_if<protocol::Welcome>(&message)) {
        if (state.inMatch) {
            state.team = welcome->team;
        }
    } else if (const auto* status = std::get_if<protocol::MatchStatus>(&message)) {
        if (state.inMatch && status->phase == protocol::MatchPhase::Ended && !state.endedShown) {
            state.final = *status;
            state.endedShown = true;
            engine.getPlugin<scene::ScenePlugin>().manager().push<EndedScene>();
        }
    }
}

void MenuScenesPlugin::onJoinResult(engine::Engine& engine, const protocol::JoinResult& result) {
    scene::SceneManager& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
    auto& state = engine.registry().getResource<MenuState>();
    if (state.cancelledRequests > 0) {
        // The answer to a request the player cancelled: drop it, and leave the match it put the player in.
        --state.cancelledRequests;
        if (result.status == protocol::JoinStatus::Joined) {
            send(engine, protocol::encode(protocol::LeaveMatch{}));
        }
        return;
    }
    state.requestPending = false;
    if (result.status != protocol::JoinStatus::Joined) {
        if (scenes.top() == scenes.idOf<JoiningScene>()) {
            state.notify(std::string(refusal(result.status)));
            scenes.pop();
        }
        return;
    }
    // Joined, even if the wait timed out: the server put the player in the match, follow it.
    if (state.inMatch) {
        return;
    }
    state.inMatch = true;
    state.endedShown = false;
    state.team.reset();
    _link->setMatch(result.matchId);
    scenes.switchTo<MatchScene>();
    engine.eventBus().publish(MatchJoined{.matchId = result.matchId});
}

void MenuScenesPlugin::update(engine::Engine& engine, const ecs::time::Time& time) {
    auto& state = engine.registry().getResource<MenuState>();
    if (state.noticeTime > 0.0F) {
        state.noticeTime -= time.deltaTime;
        if (state.noticeTime <= 0.0F) {
            state.notice.clear();
        }
    }
}

void MenuScenesPlugin::shutdown(engine::Engine& engine) {
    if (_packets) {
        engine.eventBus().unsubscribe<ecs::net::PacketReceived>(*_packets);
        _packets.reset();
    }
}

}  // namespace rlss::client
