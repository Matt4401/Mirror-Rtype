#include <gtest/gtest.h>
#include <raylib.h>

#include <cmath>

#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "events/InputActionEvent.hpp"
#include "input/ActionId.hpp"
#include "input/ActionMapping.hpp"
#include "input/InputConstants.hpp"
#include "input/InputState.hpp"
#include "input/KeyboardLayout.hpp"
#include "input/RTypeBindings.hpp"
#include "systems/input/InputSystem.hpp"

TEST(ActionId, Fnv1aHashConsistency) {
    constexpr input::ActionId shootId = input::makeActionId("shoot");
    EXPECT_EQ(shootId, input::makeActionId("shoot"));
    EXPECT_NE(shootId, input::makeActionId("move_x"));
    EXPECT_EQ(input::FNV1A_OFFSET_BASIS_32, 2166136261U);
    EXPECT_EQ(input::FNV1A_PRIME_32, 16777619U);
}

TEST(ActionMapping, DefaultConstructorIsEmpty) {
    const input::ActionMapping mapping;

    EXPECT_FALSE(mapping.hasKey("move_up", KEY_W));
    EXPECT_FALSE(mapping.hasKey("move_up", KEY_Z));
    EXPECT_FALSE(mapping.hasKey("shoot", KEY_SPACE));
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, RuntimeBindingAndRemapping) {
    input::ActionMapping mapping;

    mapping.registerAction("shoot", input::ActionType::Button);
    EXPECT_TRUE(mapping.bindKey("shoot", KEY_E));
    EXPECT_TRUE(mapping.hasKey("shoot", KEY_E));

    EXPECT_TRUE(mapping.unbindKey("shoot", KEY_E));
    EXPECT_FALSE(mapping.hasKey("shoot", KEY_E));

    mapping.registerAction("move_up", input::ActionType::Button);
    EXPECT_TRUE(mapping.bindKey("move_up", KEY_W));
    EXPECT_TRUE(mapping.bindKey("move_up", KEY_Z));
    EXPECT_TRUE(mapping.hasKey("move_up", KEY_W));
    EXPECT_TRUE(mapping.hasKey("move_up", KEY_Z));

    mapping.clearKeyBindings("move_up");
    EXPECT_FALSE(mapping.hasKey("move_up", KEY_W));
    EXPECT_FALSE(mapping.hasKey("move_up", KEY_Z));

    mapping.setDeadzone(0.35F);
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.35F);

    mapping.clear();
    EXPECT_FLOAT_EQ(mapping.deadzone(), 0.2F);
}

TEST(ActionMapping, CompositeAxisConfiguration) {
    input::ActionMapping mapping;
    mapping.registerAction("move_x", input::ActionType::Axis);
    mapping.bindAxisComposite("move_x", KEY_A, KEY_D);

    const auto* binding = mapping.getBinding(input::makeActionId("move_x"));
    ASSERT_NE(binding, nullptr);
    EXPECT_EQ(binding->type, input::ActionType::Axis);
    EXPECT_EQ(binding->negativeKeys.at(0), KEY_A);
    EXPECT_EQ(binding->keys.at(0), KEY_D);
}

TEST(InputState, DynamicActionsAndAxes) {
    input::InputState state;

    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_FALSE(state.isJustPressed("shoot"));
    EXPECT_FALSE(state.isJustReleased("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.0F);
    EXPECT_FLOAT_EQ(state.getAxis("move_y"), 0.0F);

    state.setDown("shoot", true);
    state.setJustPressed("shoot", true);
    state.setAxis("move_x", 0.707F);
    state.setAxis("move_y", -0.707F);

    EXPECT_TRUE(state.isDown("shoot"));
    EXPECT_TRUE(state.isJustPressed("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.707F);
    EXPECT_FLOAT_EQ(state.getAxis("move_y"), -0.707F);

    state.resetFrameTransitions();
    EXPECT_TRUE(state.isDown("shoot"));
    EXPECT_FALSE(state.isJustPressed("shoot"));

    state.setDown("shoot", false);
    state.setJustReleased("shoot", true);
    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_TRUE(state.isJustReleased("shoot"));

    state.clear();
    EXPECT_FALSE(state.isDown("shoot"));
    EXPECT_FLOAT_EQ(state.getAxis("move_x"), 0.0F);
}

TEST(InputState, Vector2Normalization) {
    input::InputState state;
    state.setAxis("move_x", 1.0F);
    state.setAxis("move_y", 1.0F);

    // Without normalization, magnitude is sqrt(2) ~ 1.414
    const auto unnormalized = state.getVector2("move_x", "move_y", false);
    EXPECT_FLOAT_EQ(unnormalized.x, 1.0F);
    EXPECT_FLOAT_EQ(unnormalized.y, 1.0F);

    // With normalization, magnitude is clamped to 1.0F
    const auto normalized = state.getVector2("move_x", "move_y", true);
    const float length = std::sqrt((normalized.x * normalized.x) + (normalized.y * normalized.y));
    EXPECT_NEAR(length, 1.0F, 0.0001F);
    EXPECT_NEAR(normalized.x, 0.7071F, 0.001F);
    EXPECT_NEAR(normalized.y, 0.7071F, 0.001F);
}

TEST(InputActionEvent, DesignatedInitializers) {
    const events::InputActionEvent event{
        .action = input::makeActionId("shoot"),
        .state = input::ActionState::Pressed,
        .value = 1.0F,
        .playerIndex = 1,
    };
    EXPECT_EQ(event.action, input::makeActionId("shoot"));
    EXPECT_EQ(event.state, input::ActionState::Pressed);
    EXPECT_FLOAT_EQ(event.value, 1.0F);
    EXPECT_EQ(event.playerIndex, 1);
}

TEST(InputSystem, UpdatesComponentForMatchingPlayerIndex) {
    ecs::Registry registry;
    ecs::EventBus eventBus;
    systems::InputSystem system0(input::ActionMapping{}, 0, 0);
    systems::InputSystem system1(input::ActionMapping{}, 1, 1);

    const ecs::Entity player0 = registry.spawnEntity();
    registry.addComponent<input::InputState>(player0, input::InputState{.playerIndex = 0, .actions = {}});

    const ecs::Entity player1 = registry.spawnEntity();
    registry.addComponent<input::InputState>(player1, input::InputState{.playerIndex = 1, .actions = {}});

    system0.update(registry, &eventBus);
    system1.update(registry, &eventBus);

    const auto& p0State = registry.getComponents<input::InputState>().get(player0);
    EXPECT_EQ(p0State.playerIndex, 0);

    const auto& p1State = registry.getComponents<input::InputState>().get(player1);
    EXPECT_EQ(p1State.playerIndex, 1);
}

TEST(InputSystem, DoesNothingWhenNoEntityHasInputState) {
    ecs::Registry registry;
    ecs::EventBus eventBus;
    systems::InputSystem system(input::ActionMapping{}, 0, 0);

    bool eventTriggered = false;
    eventBus.subscribe<events::InputActionEvent>(
        [&eventTriggered](const events::InputActionEvent& /*event*/) { eventTriggered = true; });

    system.update(registry, &eventBus);
    EXPECT_FALSE(registry.hasResource<input::InputState>());
    EXPECT_FALSE(eventTriggered);
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

TEST(KeyboardLayout, DetectLayoutAndString) {
    EXPECT_STREQ(input::keyboardLayoutToString(input::KeyboardLayout::Qwerty), "QWERTY");
    EXPECT_STREQ(input::keyboardLayoutToString(input::KeyboardLayout::Azerty), "AZERTY");
    EXPECT_STREQ(input::keyboardLayoutToString(static_cast<input::KeyboardLayout>(99)), "UNKNOWN");

    const input::KeyboardLayout detected = input::detectKeyboardLayout();
    EXPECT_TRUE(detected == input::KeyboardLayout::Qwerty || detected == input::KeyboardLayout::Azerty);
}

TEST(KeyboardLayout, DetailIsAzertyLocale) {
    EXPECT_TRUE(input::detail::isAzertyLocale("fr_FR.UTF-8"));
    EXPECT_TRUE(input::detail::isAzertyLocale("fr_BE.UTF-8"));
    EXPECT_TRUE(input::detail::isAzertyLocale("fr-FR"));
    EXPECT_TRUE(input::detail::isAzertyLocale("fr-BE"));
    EXPECT_FALSE(input::detail::isAzertyLocale("fr_CA.UTF-8"));
    EXPECT_FALSE(input::detail::isAzertyLocale("fr-CA"));
    EXPECT_FALSE(input::detail::isAzertyLocale("fr_CH.UTF-8"));
    EXPECT_FALSE(input::detail::isAzertyLocale("fr_LU.UTF-8"));
    EXPECT_FALSE(input::detail::isAzertyLocale("en_US.UTF-8"));
    EXPECT_FALSE(input::detail::isAzertyLocale("C"));
    EXPECT_FALSE(input::detail::isAzertyLocale(""));

    EXPECT_EQ(input::WIN_LANGID_FRENCH_FRANCE, 0x040CU);
    EXPECT_EQ(input::WIN_LANGID_FRENCH_BELGIUM, 0x080CU);
}

TEST(KeyboardLayout, DetailDetectFromLinuxConfig) {
#ifdef __linux__
    const auto layout = input::detail::detectFromLinuxConfig();
    if (layout.has_value()) {
        EXPECT_TRUE(*layout == input::KeyboardLayout::Azerty || *layout == input::KeyboardLayout::Qwerty);
    }
#endif
}

TEST(RTypeBindings, SetupDefaultBindings) {
    input::ActionMapping mapping;
    client::input::setupDefaultBindings(mapping);

    // Spatial cluster keys: KEY_A (physical Left) and KEY_W (physical Up)
    EXPECT_TRUE(mapping.hasKey("move_x", KEY_A));
    EXPECT_TRUE(mapping.hasKey("move_x", KEY_D));
    EXPECT_TRUE(mapping.hasKey("move_x", KEY_LEFT));
    EXPECT_TRUE(mapping.hasKey("move_x", KEY_RIGHT));
    // Physical 'A' on AZERTY (KEY_Q) and physical 'W' on AZERTY (KEY_Z) must NOT move the ship
    EXPECT_FALSE(mapping.hasKey("move_x", KEY_Q));

    EXPECT_TRUE(mapping.hasKey("move_y", KEY_W));
    EXPECT_TRUE(mapping.hasKey("move_y", KEY_S));
    EXPECT_TRUE(mapping.hasKey("move_y", KEY_UP));
    EXPECT_TRUE(mapping.hasKey("move_y", KEY_DOWN));
    EXPECT_FALSE(mapping.hasKey("move_y", KEY_Z));

    EXPECT_TRUE(mapping.hasKey("shoot", KEY_SPACE));
    EXPECT_TRUE(mapping.hasKey("charge_weapon", KEY_LEFT_SHIFT));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
