#include <gtest/gtest.h>

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/ScriptSystem.hpp"

struct Position {
    float x = 0.0F;
    float y = 0.0F;
};

struct CollisionEvent {
    ecs::Entity entityA;
    ecs::Entity entityB;
};

TEST(Scripting, ComponentBinderDirectAccess) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.register_component<Position>();
    registry.register_component<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    scripting::ComponentBinder<Position> binder(scriptSystem.get_vm(), "core", "Position");
    binder.property<&Position::x>("x");
    binder.property<&Position::y>("y");

    std::string wrenPositionClass = binder.generate_wren_class();
    EXPECT_TRUE(scriptSystem.get_vm().interpret("core", wrenPositionClass));

    ecs::Entity const e1 = registry.spawn_entity();
    registry.add_component<Position>(e1, Position{.x = 10.0F, .y = 20.0F});

    const char* scriptCode =
        "import \"core\" for Position\n"
        "class TestScript {\n"
        "    construct new(entityId) {\n"
        "        _pos = Position.new(entityId)\n"
        "    }\n"
        "    modifyPosition() {\n"
        "        _pos.x = 42.0\n"
        "        _pos.y = _pos.y + 5.0\n"
        "    }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.get_vm().interpret("game", scriptCode));

    registry.add_component<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "game",
                                                                          .className = "TestScript",
                                                                          .instanceHandle = nullptr,
                                                                          .hasUpdate = false,
                                                                          .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.get_components<ecs::ScriptComponent>().get(e1);
    EXPECT_TRUE(script.initialized);
    EXPECT_NE(script.instanceHandle, nullptr);

    auto modifyHandle = scriptSystem.get_vm().make_call_handle("modifyPosition()");
    wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.get_vm().call(modifyHandle), WREN_RESULT_SUCCESS);

    auto& pos = registry.get_components<Position>().get(e1);
    EXPECT_EQ(pos.x, 42.0F);
    EXPECT_EQ(pos.y, 25.0F);
}

TEST(Scripting, DeferredCommandBufferDestruction) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.register_component<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    ecs::Entity const e1 = registry.spawn_entity();

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "class SuicidalScript {\n"
        "    construct new(entityId) {\n"
        "        _entity = Entity.new(entityId)\n"
        "    }\n"
        "    die() {\n"
        "        _entity.destroy()\n"
        "    }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.get_vm().interpret("gameplay", scriptCode));

    registry.add_component<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                          .className = "SuicidalScript",
                                                                          .instanceHandle = nullptr,
                                                                          .hasUpdate = false,
                                                                          .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.get_components<ecs::ScriptComponent>().get(e1);
    auto dieHandle = scriptSystem.get_vm().make_call_handle("die()");
    wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.get_vm().call(dieHandle), WREN_RESULT_SUCCESS);

    // Entity is NOT destroyed immediately (deferred safely)
    EXPECT_FALSE(scriptSystem.get_command_buffer().empty());
    EXPECT_TRUE(registry.get_components<ecs::ScriptComponent>().contains(e1));

    // Next update flushes the command buffer
    scriptSystem.update(0.016F);

    EXPECT_TRUE(scriptSystem.get_command_buffer().empty());
    EXPECT_FALSE(registry.get_components<ecs::ScriptComponent>().contains(e1));
}

TEST(Scripting, EventForwardingMagicMethod) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.register_component<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    scriptSystem.register_event_forwarding<CollisionEvent>(
        bus, "onCollision", [](const CollisionEvent& ev) { return std::make_pair(ev.entityA, ev.entityB); });

    const char* scriptCode =
        "class BumperScript {\n"
        "    construct new(entityId) {\n"
        "        _hitOther = -1\n"
        "    }\n"
        "    onCollision(otherId) {\n"
        "        _hitOther = otherId\n"
        "    }\n"
        "    hitOther { _hitOther }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.get_vm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawn_entity();
    ecs::Entity const e2 = registry.spawn_entity();

    registry.add_component<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                          .className = "BumperScript",
                                                                          .instanceHandle = nullptr,
                                                                          .hasUpdate = false,
                                                                          .initialized = false});

    scriptSystem.update(0.016F);

    bus.publish(CollisionEvent{.entityA = e1, .entityB = e2});

    auto& script = registry.get_components<ecs::ScriptComponent>().get(e1);
    auto getHitOther = scriptSystem.get_vm().make_call_handle("hitOther");
    wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.get_vm().call(getHitOther), WREN_RESULT_SUCCESS);
    double const hit = wrenGetSlotDouble(scriptSystem.get_vm().raw(), 0);

    EXPECT_EQ(hit, static_cast<double>(e2));
}

TEST(Scripting, CoroutineFiberWait) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.register_component<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    const char* scriptCode =
        "import \"core/Engine\" for Engine\n"
        "class BossScript {\n"
        "    construct new(entityId) {\n"
        "        _state = 0\n"
        "    }\n"
        "    startSequence() {\n"
        "        Fiber.new {\n"
        "            _state = 1\n"
        "            Engine.wait(1.0)\n"
        "            _state = 2\n"
        "        }.call()\n"
        "    }\n"
        "    state { _state }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.get_vm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawn_entity();
    registry.add_component<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                          .className = "BossScript",
                                                                          .instanceHandle = nullptr,
                                                                          .hasUpdate = false,
                                                                          .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.get_components<ecs::ScriptComponent>().get(e1);
    auto startHandle = scriptSystem.get_vm().make_call_handle("startSequence()");
    wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.get_vm().call(startHandle), WREN_RESULT_SUCCESS);

    auto stateHandle = scriptSystem.get_vm().make_call_handle("state");
    auto checkState = [&](double expected) {
        wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.get_vm().call(stateHandle), WREN_RESULT_SUCCESS);
        EXPECT_EQ(wrenGetSlotDouble(scriptSystem.get_vm().raw(), 0), expected);
    };

    checkState(1.0);

    // After 0.5s, still waiting in fiber
    scriptSystem.update(0.5F);
    checkState(1.0);

    // After another 0.6s (total > 1.0s), fiber wakes up and advances to state 2
    scriptSystem.update(0.6F);
    checkState(2.0);
}

TEST(Scripting, HybridOptionalUpdateLoop) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.register_component<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    const char* scriptCode =
        "class ContinuousMover {\n"
        "    construct new(entityId) {\n"
        "        _ticks = 0\n"
        "    }\n"
        "    update(dt) {\n"
        "        _ticks = _ticks + 1\n"
        "    }\n"
        "    ticks { _ticks }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.get_vm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawn_entity();
    registry.add_component<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                          .className = "ContinuousMover",
                                                                          .instanceHandle = nullptr,
                                                                          .hasUpdate = true,  // Opt-in to update loop
                                                                          .initialized = false});

    scriptSystem.update(0.016F);  // 1st frame: initializes & calls update once

    auto& script = registry.get_components<ecs::ScriptComponent>().get(e1);
    auto ticksHandle = scriptSystem.get_vm().make_call_handle("ticks");

    auto getTicks = [&]() -> double {
        wrenEnsureSlots(scriptSystem.get_vm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.get_vm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.get_vm().call(ticksHandle), WREN_RESULT_SUCCESS);
        return wrenGetSlotDouble(scriptSystem.get_vm().raw(), 0);
    };

    EXPECT_EQ(getTicks(), 1.0);

    scriptSystem.update(0.016F);  // 2nd frame
    EXPECT_EQ(getTicks(), 2.0);

    scriptSystem.update(0.016F);  // 3rd frame
    EXPECT_EQ(getTicks(), 3.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
