#include "PlayerInputPlugin.hpp"

#include <algorithm>
#include <utility>

#include "ActionId.hpp"
#include "components/InputBuffer.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "core/services/InputCodes.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "game/Components.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputPlugin.hpp"
#include "input/InputState.hpp"
#include "net/ClientReplicationPlugin.hpp"
#include "net/ReplicationEvents.hpp"

namespace rlss::client {

namespace {

/** @brief `input::InputPlugin` samples the state whose `playerIndex` is 0 by default. */
constexpr int LOCAL_PLAYER = 0;

}  // namespace

input::ActionMapping makeControls() {
    input::ActionMapping mapping;
    mapping.registerAction(game::ACTION_STEER, input::ActionType::Axis);
    mapping.bindAxisComposite(game::ACTION_STEER, engine::Key::A, engine::Key::D);
    mapping.bindAxisComposite(game::ACTION_STEER, engine::Key::Q, engine::Key::D);
    mapping.bindAxisComposite(game::ACTION_STEER, engine::Key::Left, engine::Key::Right);
    mapping.bindGamepadAxis(game::ACTION_STEER, engine::GamepadAxis::LeftX, 1.0F);

    mapping.registerAction(game::ACTION_JUMP, input::ActionType::Button);
    mapping.bindKey(game::ACTION_JUMP, engine::Key::Space);
    mapping.bindKey(game::ACTION_JUMP, engine::Key::W);
    mapping.bindKey(game::ACTION_JUMP, engine::Key::Z);
    mapping.bindKey(game::ACTION_JUMP, engine::Key::Up);
    mapping.bindGamepadButton(game::ACTION_JUMP, engine::GamepadButton::RightFaceDown);

    mapping.registerAction(game::ACTION_BOOST, input::ActionType::Button);
    mapping.bindKey(game::ACTION_BOOST, engine::Key::LeftShift);
    mapping.bindKey(game::ACTION_BOOST, engine::Key::RightShift);
    mapping.bindKey(game::ACTION_BOOST, engine::Key::S);
    mapping.bindKey(game::ACTION_BOOST, engine::Key::Down);
    mapping.bindGamepadButton(game::ACTION_BOOST, engine::GamepadButton::RightFaceRight);
    mapping.bindGamepadButton(game::ACTION_BOOST, engine::GamepadButton::RightTrigger2);
    return mapping;
}

void PlayerInputPlugin::build(engine::PluginBuilder& builder) {
    builder.require<input::InputPlugin>();
    builder.require<ecs::net::ClientReplicationPlugin>();
}

void PlayerInputPlugin::init(engine::Engine& engine) {
    input::InputState state;
    state.playerIndex = LOCAL_PLAYER;
    const ecs::Entity entity = engine.registry().spawnEntity();
    engine.registry().addComponent(entity, std::move(state));
}

void PlayerInputPlugin::fixedUpdate(engine::Engine& engine, const ecs::time::Time& /*time*/) {
    ecs::NetInput input{.sequence = ++_sequence, .held = {}, .axes = {}};
    engine.registry().runSystem<input::InputState>([&input](ecs::Entity /*entity*/, const input::InputState& state) {
        if (state.playerIndex != LOCAL_PLAYER) {
            return;
        }
        input.axes.push_back(
            {.action = game::STEER_ID, .value = std::clamp(state.getAxis(game::STEER_ID), -1.0F, 1.0F)});
        if (state.isDown(game::BOOST_ID)) {
            input.held.push_back(game::BOOST_ID);
        }
        if (state.isDown(game::JUMP_ID)) {
            input.held.push_back(game::JUMP_ID);
        }
    });
    engine.eventBus().publish(ecs::net::LocalInput{.input = std::move(input)});
}

}  // namespace rlss::client
