#include <gtest/gtest.h>
#include <raylib.h>

#include "components/InputReceiver.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/InputActionEvent.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputState.hpp"
#include "systems/input/InputSystem.hpp"

TEST(ActionMapping, DefaultBindingsIncludeInternationalAndArrows) {
    const input::ActionMapping mapping;

    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_W));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_Z));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_UP));

    EXPECT_TRUE(mapping.hasKey(input::Action::MoveDown, KEY_S));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveDown, KEY_DOWN));

    EXPECT_TRUE(mapping.hasKey(input::Action::MoveLeft, KEY_A));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveLeft, KEY_Q));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveLeft, KEY_LEFT));

    EXPECT_TRUE(mapping.hasKey(input::Action::MoveRight, KEY_D));
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveRight, KEY_RIGHT));

    EXPECT_TRUE(mapping.hasKey(input::Action::Shoot, KEY_SPACE));
    EXPECT_TRUE(mapping.hasKey(input::Action::Shoot, KEY_ENTER));

    EXPECT_TRUE(mapping.hasKey(input::Action::ChargeWeapon, KEY_LEFT_SHIFT));
    EXPECT_TRUE(mapping.hasKey(input::Action::ChargeWeapon, KEY_RIGHT_SHIFT));

    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, RuntimeRemappingForAccessibility) {
    input::ActionMapping mapping;

    EXPECT_TRUE(mapping.bindKey(input::Action::Shoot, KEY_E));
    EXPECT_TRUE(mapping.hasKey(input::Action::Shoot, KEY_E));

    EXPECT_TRUE(mapping.unbindKey(input::Action::Shoot, KEY_SPACE));
    EXPECT_FALSE(mapping.hasKey(input::Action::Shoot, KEY_SPACE));

    mapping.clearKeyBindings(input::Action::MoveUp);
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_W));
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_Z));
    EXPECT_FALSE(mapping.hasKey(input::Action::MoveUp, KEY_UP));

    mapping.setDeadzone(0.35F);
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.35F);

    mapping.resetToDefaults();
    EXPECT_TRUE(mapping.hasKey(input::Action::MoveUp, KEY_W));
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

TEST(InputReceiver, DefaultValuesAndDesignatedInitializers) {
    const ecs::InputReceiver defaultReceiver;
    EXPECT_EQ(defaultReceiver.playerIndex, 0);
    EXPECT_TRUE(defaultReceiver.enabled);
    EXPECT_FLOAT_EQ(defaultReceiver.moveX, 0.0F);
    EXPECT_FLOAT_EQ(defaultReceiver.moveY, 0.0F);
    EXPECT_FALSE(defaultReceiver.shooting);
    EXPECT_FALSE(defaultReceiver.charging);
    EXPECT_FLOAT_EQ(defaultReceiver.speed, 350.0F);

    const ecs::InputReceiver customReceiver{
        .playerIndex = 2,
        .enabled = false,
        .moveX = -1.0F,
        .moveY = 1.0F,
        .shooting = true,
        .charging = true,
        .speed = 500.0F,
    };
    EXPECT_EQ(customReceiver.playerIndex, 2);
    EXPECT_FALSE(customReceiver.enabled);
    EXPECT_FLOAT_EQ(customReceiver.moveX, -1.0F);
    EXPECT_FLOAT_EQ(customReceiver.moveY, 1.0F);
    EXPECT_TRUE(customReceiver.shooting);
    EXPECT_TRUE(customReceiver.charging);
    EXPECT_FLOAT_EQ(customReceiver.speed, 500.0F);
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

TEST(InputSystem, AutomaticallyInitializesResourceAndUpdatesReceiver) {
    ecs::Registry registry;
    ecs::EventBus eventBus;
    systems::InputSystem system;

    const ecs::Entity player = registry.spawnEntity();
    registry.addComponent<ecs::InputReceiver>(player, ecs::InputReceiver{.speed = 400.0F});

    EXPECT_FALSE(registry.hasResource<input::InputState>());

    system.update(registry, &eventBus);

    EXPECT_TRUE(registry.hasResource<input::InputState>());
    const auto& state = registry.getResource<input::InputState>();
    EXPECT_FLOAT_EQ(state.axisX(), 0.0F);
    EXPECT_FLOAT_EQ(state.axisY(), 0.0F);

    const auto& receiver = registry.getComponents<ecs::InputReceiver>().get(player);
    EXPECT_FLOAT_EQ(receiver.speed, 400.0F);
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

TEST(InputSystem, OnlyUpdatesReceiverMatchingPlayerIndex) {
    ecs::Registry registry;
    ecs::EventBus eventBus;

    const ecs::Entity player0 = registry.spawnEntity();
    registry.addComponent<ecs::InputReceiver>(
        player0, ecs::InputReceiver{.playerIndex = 0, .moveX = 0.0F, .moveY = 0.0F, .shooting = false});

    const ecs::Entity player1 = registry.spawnEntity();
    registry.addComponent<ecs::InputReceiver>(
        player1, ecs::InputReceiver{.playerIndex = 1, .moveX = 0.0F, .moveY = 0.0F, .shooting = false});

    // Run system configured specifically for player 1
    systems::InputSystem systemPlayer1(input::ActionMapping{}, 1, 0);
    systemPlayer1.update(registry, &eventBus);

    // Both should exist, but player 0's receiver is filtered out by player 1's system
    const auto& r0 = registry.getComponents<ecs::InputReceiver>().get(player0);
    const auto& r1 = registry.getComponents<ecs::InputReceiver>().get(player1);
    EXPECT_EQ(r0.playerIndex, 0);
    EXPECT_EQ(r1.playerIndex, 1);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
