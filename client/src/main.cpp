/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <cstddef>
#include <exception>
#include <iostream>
#include <span>

#include "ClientOptions.hpp"
#include "RTypeGamePlugin.hpp"
#include "audio/AudioPlugin.hpp"
#include "core/Engine.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "input/InputPlugin.hpp"
#include "metrics/MetricsPlugin.hpp"
#include "metrics/ShipMetricsPlugin.hpp"
#include "metrics/overlay/MetricsOverlayPlugin.hpp"
#include "net/AutoLobbyPlugin.hpp"
#include "net/ClientNetwork.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "render/RenderPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "ui/UiPlugin.hpp"

namespace {

constexpr int EXIT_ERROR = 84;

}  // namespace

int main(int ac, char** av) {
    const std::span args(av, static_cast<std::size_t>(ac));
    const auto options = client::parseOptions(args);
    if (!options) {
        std::cerr << "usage: " << args.front() << " [host] [port]\n";
        return EXIT_ERROR;
    }
    try {
        engine::Engine engine;
        engine.addPlugin<render::RenderPlugin>()
            .addPlugin<input::InputPlugin>()
            .addPlugin<audio::AudioPlugin>()
            .addPlugin<systems::PhysicsPlugin>()
            .addPlugin<systems::AnimationPlugin>()
            .addPlugin<prefabs::PrefabPlugin>()
            .addPlugin<scripting::ScriptPlugin>()
            .addPlugin<hierarchy::HierarchyPlugin>()
            .addPlugin<ui::UiPlugin>()
            .addPlugin<metrics::MetricsPlugin>()
            .addPlugin<metrics::MetricsOverlayPlugin>()
            .addPlugin<client::ShipMetricsPlugin>()
            .addPlugin<client::RTypeGamePlugin>();
        client::addNetworkPlugins(engine);
        engine.addPlugin<client::AutoLobbyPlugin>(*options);

        engine.run();
        return engine.getPlugin<client::AutoLobbyPlugin>().failed() ? EXIT_ERROR : 0;
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return EXIT_ERROR;
    }
}
