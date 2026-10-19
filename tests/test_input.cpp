#include <gtest/gtest.h>
#include <raylib.h>

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/InputActionEvent.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputState.hpp"
#include "systems/input/InputSystem.hpp"

TEST(ActionMapping, DefaultConstructorIsEmpty) {
    const input::ActionMapping mapping;

    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_W));
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_Z));
    EXPECT_FALSE(mapping.hasKey(input::Action::Shoot, KEY_SPACE));
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, RuntimeBindingAndRemapping) {
    input::ActionMapping mapping;

    EXPECT_TRUE(mapping.bindKey(input::Action::Shoot, KEY_E));
    EXPECT_TRUE(mapping.hasKey(input::Action::Shoot, KEY_E));

    EXPECT_TRUE(mapping.unbindKey(input::Action::Shoot, KEY_E));
    EXPECT_FALSE(mapping.hasKey(input::Action::Shoot, KEY_E));

    EXPECT_TRUE(mapping.bindKey(input::Action::MoveUp, KEY_W));
    EXPECT_TRUE(mapping.bindKey(input::Action::MoveUp, KEY_Z));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_W));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_Z));

    mapping.clearKeyBindings(input::Action::MoveUp);
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_W));
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_Z));

    mapping.setDeadzone(0.35F);
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.35F);

    mapping.clear();
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(InputState, BitsetFlagsAndAxes) {
    input::InputState state;

    EXPECT_FALSE(state.isDown(input::Action::Shoot));
    EXPECT_FALSE(state.isJustPressed(input::Action::Shoot));
    EXPECT_FALSE(state.isJustReleased(input::Action::Shoot));
    EXPECT_FLOAT_EQ(state.axisX(), 0.0F);
    EXPECT_FLOAT_EQ(state.axisY(), 0.0F);

    state.setDown(input::Action::Shoot, true);
    state.setJustPressed(input::Action::Shoot, true);
    state.setAxes(0.707F, -0.707F);

    EXPECT_TRUE(state.isDown(input::Action::Shoot));
    EXPECT_TRUE(state.isJustPressed(input::Action::Shoot));
    EXPECT_FLOAT_EQ(state.axisX(), 0.707F);
    EXPECT_FLOAT_EQ(state.axisY(), -0.707F);

    state.resetFrameTransitions();
    EXPECT_TRUE(state.isDown(input::Action::Shoot));
    EXPECT_FALSE(state.isJustPressed(input::Action::Shoot));
}

TEST(InputActionEvent, DesignatedInitializers) {
    const events::InputActionEvent event{
        .action = input::Action::Shoot,
        .state = input::ActionState::Pressed,
        .value = 1.0F,
        .playerIndex = 1,
    };
    EXPECT_EQ(event.action, input::Action::Shoot);
    EXPECT_EQ(event.state, input::ActionState::Pressed);
    EXPECT_FLOAT_EQ(event.value, 1.0F);
    EXPECT_EQ(event.playerIndex, 1);
}

TEST(InputSystem, AutomaticallyInitializesResource) {
    ecs::Registry registry;
    ecs::EventBus eventBus;
    systems::InputSystem system;

    EXPECT_FALSE(registry.hasResource<input::InputState>());

    system.update(registry, &eventBus);

    EXPECT_TRUE(registry.hasResource<input::InputState>());
    const auto& state = registry.getResource<input::InputState>();
    EXPECT_FLOAT_EQ(state.axisX(), 0.0F);
    EXPECT_FLOAT_EQ(state.axisY(), 0.0F);
}

TEST(InputSystem, ConfigurablePlayerAndGamepadIndices) {
    const systems::InputSystem defaultSystem;
    EXPECT_EQ(defaultSystem.playerIndex(), 0);
    EXPECT_EQ(defaultSystem.gamepadIndex(), 0);

    systems::InputSystem customSystem(input::ActionMapping{}, 2, 1);
    EXPECT_EQ(customSystem.playerIndex(), 2);
    EXPECT_EQ(customSystem.gamepadIndex(), 1);

    customSystem.setPlayerIndex(3);
    customSystem.setGamepadIndex(2);
    EXPECT_EQ(customSystem.playerIndex(), 3);
    EXPECT_EQ(customSystem.gamepadIndex(), 2);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
