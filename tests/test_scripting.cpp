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
    registry.registerComponent<Position>();
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    scripting::ComponentBinder<Position> binder(scriptSystem.getVm(), "core/Position", "Position");
    binder.property<&Position::x>("x");
    binder.property<&Position::y>("y");

    std::string wrenPositionClass = binder.generateWrenClass();
    EXPECT_TRUE(scriptSystem.getVm().interpret("core/Position", wrenPositionClass));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<Position>(e1, Position{.x = 10.0F, .y = 20.0F});

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "import \"core/Position\" for Position\n"
        "class TestScript {\n"
        "    construct new(entity) {\n"
        "        _entity = entity\n"
        "        _pos = Position.new(entity.id)\n"
        "    }\n"
        "    modifyPosition() {\n"
        "        _pos.x = 42.0\n"
        "        _pos.y = _pos.y + 5.0\n"
        "    }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("game", scriptCode));

    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "game",
                                                                         .className = "TestScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    EXPECT_TRUE(script.initialized);
    EXPECT_NE(script.instanceHandle, nullptr);

    auto modifyHandle = scriptSystem.getVm().makeCallHandle("modifyPosition()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(modifyHandle), WREN_RESULT_SUCCESS);

    auto& pos = registry.getComponents<Position>().get(e1);
    EXPECT_EQ(pos.x, 42.0F);
    EXPECT_EQ(pos.y, 25.0F);
}

TEST(Scripting, DeferredCommandBufferDestruction) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    ecs::Entity const e1 = registry.spawnEntity();

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "class SuicidalScript {\n"
        "    construct new(entity) {\n"
        "        _entity = entity\n"
        "    }\n"
        "    die() {\n"
        "        _entity.destroy()\n"
        "    }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "SuicidalScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto dieHandle = scriptSystem.getVm().makeCallHandle("die()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(dieHandle), WREN_RESULT_SUCCESS);

    // Entity is NOT destroyed immediately (deferred safely)
    EXPECT_FALSE(scriptSystem.getCommandBuffer().empty());
    EXPECT_TRUE(registry.getComponents<ecs::ScriptComponent>().contains(e1));

    // Next update flushes the command buffer
    scriptSystem.update(0.016F);

    EXPECT_TRUE(scriptSystem.getCommandBuffer().empty());
    EXPECT_FALSE(registry.getComponents<ecs::ScriptComponent>().contains(e1));
}

TEST(Scripting, EventForwardingMagicMethod) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    scriptSystem.registerEventForwarding<CollisionEvent>(
        bus, "onCollision", [](const CollisionEvent& ev) { return std::make_pair(ev.entityA, ev.entityB); });

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "class BumperScript {\n"
        "    construct new(entity) {\n"
        "        _entity = entity\n"
        "        _hitOther = null\n"
        "    }\n"
        "    onCollision(other) {\n"
        "        _hitOther = other.id\n"
        "    }\n"
        "    hitOther { _hitOther }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    ecs::Entity const e2 = registry.spawnEntity();

    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "BumperScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    bus.publish(CollisionEvent{.entityA = e1, .entityB = e2});

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto getHitOther = scriptSystem.getVm().makeCallHandle("hitOther");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(getHitOther), WREN_RESULT_SUCCESS);
    double const hit = wrenGetSlotDouble(scriptSystem.getVm().raw(), 0);

    EXPECT_EQ(hit, static_cast<double>(e2));
}

TEST(Scripting, CoroutineFiberWait) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

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

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "BossScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto startHandle = scriptSystem.getVm().makeCallHandle("startSequence()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(startHandle), WREN_RESULT_SUCCESS);

    auto stateHandle = scriptSystem.getVm().makeCallHandle("state");
    auto checkState = [&](double expected) {
        wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.getVm().call(stateHandle), WREN_RESULT_SUCCESS);
        EXPECT_EQ(wrenGetSlotDouble(scriptSystem.getVm().raw(), 0), expected);
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
    registry.registerComponent<ecs::ScriptComponent>();

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

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "ContinuousMover",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = true,  // Opt-in to update loop
                                                                         .initialized = false});

    scriptSystem.update(0.016F);  // 1st frame: initializes & calls update once

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto ticksHandle = scriptSystem.getVm().makeCallHandle("ticks");

    auto getTicks = [&]() -> double {
        wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.getVm().call(ticksHandle), WREN_RESULT_SUCCESS);
        return wrenGetSlotDouble(scriptSystem.getVm().raw(), 0);
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
