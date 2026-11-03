/*
** EPITECH PROJECT, 2026
** r-type
** File description:
** Client entry point
*/

#include <exception>
#include <iostream>

#include "RTypeGamePlugin.hpp"
#include "core/Engine.hpp"
#include "input/InputPlugin.hpp"
#include "prefabs/PrefabPlugin.hpp"
#include "render/RenderPlugin.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "systems/physics/PhysicsPlugin.hpp"
#include "systems/render/AnimationPlugin.hpp"
#include "ui/UiPlugin.hpp"

int main() {
    try {
        engine::Engine engine;
        engine.addPlugin<render::RenderPlugin>()
            .addPlugin<input::InputPlugin>()
            .addPlugin<systems::PhysicsPlugin>()
            .addPlugin<systems::AnimationPlugin>()
            .addPlugin<prefabs::PrefabPlugin>()
            .addPlugin<scripting::ScriptPlugin>()
            .addPlugin<ui::UiPlugin>()
            .addPlugin<client::RTypeGamePlugin>();

        engine.run();
    } catch (const std::exception& error) {
        std::cerr << "client error: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
