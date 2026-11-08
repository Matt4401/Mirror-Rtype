/**
 * @file main.cpp
 * @brief RLSS client entry point: `rlss_client [host] [port]`.
 */

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "ArenaViewPlugin.hpp"
#include "AssetsPlugin.hpp"
#include "BallViewPlugin.hpp"
#include "CameraPlugin.hpp"
#include "CarViewPlugin.hpp"
#include "CommandLine.hpp"
#include "GoalEffectPlugin.hpp"
#include "HudPlugin.hpp"
#include "Logger.hpp"
#include "NameTagPlugin.hpp"
#include "PlayerInputPlugin.hpp"
#include "ServerLink.hpp"
#include "SessionPlugin.hpp"
#include "SoundPlugin.hpp"
#include "VolumeCVarsPlugin.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/audio/AudioAssetsPlugin.hpp"
#include "assets/render/RenderAssetsPlugin.hpp"
#include "assets/scene/SceneAssetsPlugin.hpp"
#include "assets/scripting/AssetScriptingPlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "config/Settings.hpp"
#include "core/Engine.hpp"
#include "game/ReplicationCodec.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "input/InputPlugin.hpp"
#include "menu/MenuScenesPlugin.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/INetworkClient.hpp"
#include "net/NetworkFactory.hpp"
#include "net/NetworkPlugin.hpp"
#include "protocol/Protocol.hpp"
#include "render/RenderPlugin.hpp"
#include "render/window/WindowConfig.hpp"
#include "scene/ScenePlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "settings/SettingsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/devconsole/DevConsolePlugin.hpp"
#include "tools/scripting/ConsoleScriptingPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace {

constexpr int EXIT_ERROR = 84;
constexpr std::size_t MAX_VALUES = 2;
constexpr const char* DEFAULT_HOST = "127.0.0.1";
constexpr const char* LOG_TAG = "client";
/** @brief Longest wait for the server before giving up, the window never opening. */
constexpr std::chrono::seconds CONNECT_TIMEOUT{10};

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto values = args.subspan(1);
    std::optional<std::uint16_t> port = rlss::protocol::DEFAULT_PORT;
    if (values.size() > MAX_VALUES) {
        port.reset();
    } else if (values.size() == MAX_VALUES) {
        port = rlss::parsePort(values.back());
    }
    if (!port) {
        std::cerr << "usage: " << args.front() << " [host] [port]\n";
        return EXIT_ERROR;
    }
    const std::string host = values.empty() ? DEFAULT_HOST : values.front();

    try {
        const std::shared_ptr<ecs::net::INetworkClient> transport =
            ecs::net::createClient({.protocolId = rlss::protocol::PROTOCOL_ID});
        rtype::log::info(LOG_TAG, "connecting to ", host, ":", *port, "...");
        auto received = rlss::client::connectToServer(*transport, host, *port, CONNECT_TIMEOUT);
        if (!received) {
            rtype::log::error(LOG_TAG, "cannot connect: ", rlss::client::describe(transport->disconnectReason()));
            return EXIT_ERROR;
        }
        rtype::log::info(LOG_TAG, "connected");
        // Snapshots and inputs travel over UDP, on the same port number as the TCP connection.
        if (!transport->openUdp(*port)) {
            rtype::log::error(LOG_TAG, "cannot open the UDP channel");
            return EXIT_ERROR;
        }
        const auto link = std::make_shared<rlss::client::ServerLink>(transport, std::move(*received));

        // Connected: open the game window, on the menu.
        engine::Engine engine;
        // Escape means "back" in the menus; on the main menu and in a match it still quits.
        engine
            .addPlugin<render::RenderPlugin>(
                render::WindowConfig{.title = std::string(rlss::protocol::GAME_TITLE), .escapeClosesWindow = false})
            .addPlugin<input::InputPlugin>(rlss::client::makeControls())
            .addPlugin<systems::AnimationPlugin>()
            .addPlugin<audio::AudioPlugin>()
            .addPlugin<ecs::net::NetworkPlugin>(link, link)
            .addPlugin<ecs::net::ClientReplicationPlugin>(std::make_shared<rlss::game::ReplicationCodec>())
            // After the replication: the CarView scripts read the car transforms once interpolated.
            .addPlugin<scripting::ScriptPlugin>()
            .addPlugin<assets::AssetPlugin>()
            .addPlugin<assets::AssetScriptingPlugin>(std::string(rlss::client::ASSET_DECLARATIONS))
            .addPlugin<assets::RenderAssetsPlugin>()
            .addPlugin<assets::AudioAssetsPlugin>()
            .addPlugin<rlss::client::SessionPlugin>(transport)
            .addPlugin<rlss::client::AssetsPlugin>()
            .addPlugin<rlss::client::ArenaViewPlugin>()
            .addPlugin<rlss::client::CarViewPlugin>()
            .addPlugin<rlss::client::BallViewPlugin>()
            .addPlugin<rlss::client::CameraPlugin>()
            .addPlugin<hierarchy::HierarchyPlugin>()
            .addPlugin<ui::UiPlugin>()
            .addPlugin<scene::ScenePlugin>()
            .addPlugin<assets::SceneAssetsPlugin>()
            .addPlugin<settings::SettingsPlugin>(config::userConfigDirectory("rlss") / "settings.toml")
            // Drawn on screen in this order: name tags, HUD, then the menus on top (UiPlugin draws in lateUpdate).
            .addPlugin<rlss::client::NameTagPlugin>()
            .addPlugin<rlss::client::HudPlugin>()
            .addPlugin<rlss::client::GoalEffectPlugin>()
            .addPlugin<rlss::client::SoundPlugin>()
            .addPlugin<rlss::client::PlayerInputPlugin>()
            .addPlugin<rlss::client::MenuScenesPlugin>(link)
            // Developer console (~ / ²): drawn last, over the HUD and the menus.
            // Cheats only reach this client: the server stays authoritative and runs its own rules.
            .addPlugin<tools::ConsolePlugin>(tools::ConsoleConfig{.allowCheats = true})
            .addPlugin<tools::ConsoleScriptingPlugin>()
            .addPlugin<tools::DevConsolePlugin>()
            .addPlugin<rlss::client::VolumeCVarsPlugin>();
        engine.run();
        return engine.getPlugin<rlss::client::SessionPlugin>().connectionLost() ? EXIT_ERROR : 0;
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
}
