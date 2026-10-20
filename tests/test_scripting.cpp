#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>

#include "components/ScriptComponent.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "events/EventBus.hpp"
#include "input/InputState.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

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

TEST(Scripting, HotReloadValidCode) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<Position>();
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);
    scriptSystem.enableHotReload(true);

    auto registerPosition = [](scripting::WrenVM& vm) {
        scripting::ComponentBinder<Position> binder(vm, "core/Position", "Position");
        binder.property<&Position::x>("x");
        binder.property<&Position::y>("y");
        std::string wrenPositionClass = binder.generateWrenClass();
        EXPECT_TRUE(vm.interpret("core/Position", wrenPositionClass));
    };
    registerPosition(scriptSystem.getVm());
    scriptSystem.addBinderReplay(registerPosition);

    std::string const scriptPath = "TestReloadEntity.wren";
    {
        std::ofstream out(scriptPath);
        out << "import \"core/Entity\" for Entity\n"
            << "import \"core/Position\" for Position\n"
            << "class TestReloadEntity {\n"
            << "    construct new(entity) {\n"
            << "        _entity = entity\n"
            << "        _pos = Position.new(entity.id)\n"
            << "    }\n"
            << "    applyMove() {\n"
            << "        _pos.x = _pos.x + 1.0\n"
            << "    }\n"
            << "}\n";
    }

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<Position>(e1, Position{.x = 10.0F, .y = 20.0F});
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "TestReloadEntity",
                                                                         .className = "TestReloadEntity",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);
    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    EXPECT_TRUE(script.initialized);

    auto callApplyMove = [&]() {
        auto applyHandle = scriptSystem.getVm().makeCallHandle("applyMove()");
        wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.getVm().call(applyHandle), WREN_RESULT_SUCCESS);
    };

    callApplyMove();
    EXPECT_EQ(registry.getComponents<Position>().get(e1).x, 11.0F);

    // Modify script on disk to add 10.0 instead of 1.0
    {
        std::ofstream out(scriptPath);
        out << "import \"core/Entity\" for Entity\n"
            << "import \"core/Position\" for Position\n"
            << "class TestReloadEntity {\n"
            << "    construct new(entity) {\n"
            << "        _entity = entity\n"
            << "        _pos = Position.new(entity.id)\n"
            << "    }\n"
            << "    applyMove() {\n"
            << "        _pos.x = _pos.x + 10.0\n"
            << "    }\n"
            << "}\n";
    }
    auto const futureTime = std::filesystem::last_write_time(scriptPath) + std::chrono::seconds(2);
    std::filesystem::last_write_time(scriptPath, futureTime);

    // Timer >= 0.5F triggers reload
    scriptSystem.update(0.6F);

    callApplyMove();
    EXPECT_EQ(registry.getComponents<Position>().get(e1).x, 21.0F);
    EXPECT_EQ(registry.getComponents<Position>().get(e1).y, 20.0F);

    std::filesystem::remove(scriptPath);
}

TEST(Scripting, HotReloadSyntaxErrorResilience) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<Position>();
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);
    scriptSystem.enableHotReload(true);

    auto registerPosition = [](scripting::WrenVM& vm) {
        scripting::ComponentBinder<Position> binder(vm, "core/Position", "Position");
        binder.property<&Position::x>("x");
        binder.property<&Position::y>("y");
        std::string wrenPositionClass = binder.generateWrenClass();
        EXPECT_TRUE(vm.interpret("core/Position", wrenPositionClass));
    };
    registerPosition(scriptSystem.getVm());
    scriptSystem.addBinderReplay(registerPosition);

    std::string const scriptPath = "TestSyntaxError.wren";
    {
        std::ofstream out(scriptPath);
        out << "class TestSyntaxError {\n"
            << "    construct new(entity) {}\n"
            << "    value { 42.0 }\n"
            << "}\n";
    }

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "TestSyntaxError",
                                                                         .className = "TestSyntaxError",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);
    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    EXPECT_TRUE(script.initialized);

    auto getValue = [&]() -> double {
        auto valHandle = scriptSystem.getVm().makeCallHandle("value");
        wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.getVm().call(valHandle), WREN_RESULT_SUCCESS);
        return wrenGetSlotDouble(scriptSystem.getVm().raw(), 0);
    };

    EXPECT_EQ(getValue(), 42.0);

    // Write syntax error
    {
        std::ofstream out(scriptPath);
        out << "class TestSyntaxError { brokenCode { \n";
    }
    auto const futureTime = std::filesystem::last_write_time(scriptPath) + std::chrono::seconds(2);
    std::filesystem::last_write_time(scriptPath, futureTime);

    // Update triggers hot-reload attempt
    scriptSystem.update(0.6F);

    // The invalid code should be rejected, old script instance and VM still functional
    EXPECT_EQ(getValue(), 42.0);

    std::filesystem::remove(scriptPath);
}

namespace {
int g_destroyHookCallCount = 0;
void recordDestroyHook(::WrenVM* /*rawVm*/) { ++g_destroyHookCallCount; }

int g_reloadHookCallCount = 0;
void recordReloadHook(::WrenVM* /*rawVm*/) { ++g_reloadHookCallCount; }
}  // namespace

TEST(Scripting, LifecycleOnDestroy) {
    g_destroyHookCallCount = 0;
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    scriptSystem.getVm().registerForeignMethod("gameplay", "LifecycleDestroyScript", true, "recordDestroyed()",
                                               &recordDestroyHook);

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "class LifecycleDestroyScript {\n"
        "    construct new(entity) {\n"
        "        _entity = entity\n"
        "    }\n"
        "    onDestroy() {\n"
        "        LifecycleDestroyScript.recordDestroyed()\n"
        "    }\n"
        "    die() {\n"
        "        _entity.destroy()\n"
        "    }\n"
        "    foreign static recordDestroyed()\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "LifecycleDestroyScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto dieHandle = scriptSystem.getVm().makeCallHandle("die()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(dieHandle), WREN_RESULT_SUCCESS);

    // Destruction is deferred, onDestroy has NOT run yet
    EXPECT_EQ(g_destroyHookCallCount, 0);
    EXPECT_TRUE(registry.getComponents<ecs::ScriptComponent>().contains(e1));

    // Next update flushes command buffer and triggers onDestroy()
    scriptSystem.update(0.016F);

    EXPECT_EQ(g_destroyHookCallCount, 1);
    EXPECT_FALSE(registry.getComponents<ecs::ScriptComponent>().contains(e1));
}

TEST(Scripting, LifecycleOnReload) {
    g_reloadHookCallCount = 0;
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);
    scriptSystem.enableHotReload(true);

    auto registerForeigns = [](scripting::WrenVM& vm) {
        vm.registerForeignMethod("TestReloadHook", "ReloadHookScript", true, "recordReloaded()", &recordReloadHook);
    };
    registerForeigns(scriptSystem.getVm());
    scriptSystem.addBinderReplay(registerForeigns);

    std::string const scriptPath = "TestReloadHook.wren";
    {
        std::ofstream out(scriptPath);
        out << "class ReloadHookScript {\n"
            << "    construct new(entity) {\n"
            << "        _entity = entity\n"
            << "    }\n"
            << "    onReload() {\n"
            << "        ReloadHookScript.recordReloaded()\n"
            << "    }\n"
            << "    foreign static recordReloaded()\n"
            << "}\n";
    }

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "TestReloadHook",
                                                                         .className = "ReloadHookScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);
    // Initial spawn should NOT trigger onReload()
    EXPECT_EQ(g_reloadHookCallCount, 0);

    // Touch file to trigger reload
    auto const futureTime = std::filesystem::last_write_time(scriptPath) + std::chrono::seconds(2);
    std::filesystem::last_write_time(scriptPath, futureTime);

    // Update triggers hot-reload
    scriptSystem.update(0.6F);

    // onReload() was called once during hot-reload!
    EXPECT_EQ(g_reloadHookCallCount, 1);

    std::filesystem::remove(scriptPath);
}

TEST(Scripting, CoroutineChainedWait) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    const char* scriptCode =
        "import \"core/Engine\" for Engine\n"
        "class ChainedBossScript {\n"
        "    construct new(entityId) {\n"
        "        _step = 0\n"
        "    }\n"
        "    start() {\n"
        "        Fiber.new {\n"
        "            _step = 1\n"
        "            Engine.wait(0.1)\n"
        "            _step = 2\n"
        "            Engine.wait(0.1)\n"
        "            _step = 3\n"
        "        }.call()\n"
        "    }\n"
        "    step { _step }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                         .className = "ChainedBossScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    auto startHandle = scriptSystem.getVm().makeCallHandle("start()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(startHandle), WREN_RESULT_SUCCESS);

    auto stepHandle = scriptSystem.getVm().makeCallHandle("step");
    auto checkStep = [&](double expected) {
        wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
        wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
        EXPECT_EQ(scriptSystem.getVm().call(stepHandle), WREN_RESULT_SUCCESS);
        EXPECT_EQ(wrenGetSlotDouble(scriptSystem.getVm().raw(), 0), expected);
    };

    checkStep(1.0);

    // Resuming first wait enters second Engine.wait during updateSleepingFibers
    scriptSystem.update(0.15F);
    checkStep(2.0);

    // Resuming second wait finishes
    scriptSystem.update(0.15F);
    checkStep(3.0);
}

TEST(Scripting, TeardownHandleRelease) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<ecs::ScriptComponent>();

    const char* scriptCode =
        "class TeardownScript {\n"
        "    construct new(entityId) {}\n"
        "}\n";

    {
        systems::ScriptSystem scriptSystem;
        scriptSystem.init(registry, bus);
        EXPECT_TRUE(scriptSystem.getVm().interpret("gameplay", scriptCode));

        ecs::Entity const e1 = registry.spawnEntity();
        registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "gameplay",
                                                                             .className = "TeardownScript",
                                                                             .instanceHandle = nullptr,
                                                                             .hasUpdate = false,
                                                                             .initialized = false});

        scriptSystem.update(0.016F);
        EXPECT_NE(registry.getComponents<ecs::ScriptComponent>().get(e1).instanceHandle, nullptr);
        // scriptSystem goes out of scope here: destructor must release handles cleanly
    }

    // Handles must have been reset to nullptr upon teardown
    EXPECT_EQ(registry.getComponents<ecs::ScriptComponent>().get(0).instanceHandle, nullptr);
    EXPECT_FALSE(registry.getComponents<ecs::ScriptComponent>().get(0).initialized);
}

TEST(Scripting, InputModuleBindings) {
    ecs::Registry registry;
    ecs::EventBus bus;
    registry.registerComponent<Position>();
    registry.registerComponent<ecs::ScriptComponent>();

    systems::ScriptSystem scriptSystem;
    scriptSystem.init(registry, bus);

    const ecs::Entity player = registry.spawnEntity();
    auto& inputState =
        registry.addComponent<input::InputState>(player, input::InputState{.playerIndex = 0, .actions = {}});
    inputState.setDown("shoot", true);
    inputState.setJustPressed("shoot", true);
    inputState.setAxis("move_y", -0.75F);

    scripting::ComponentBinder<Position> binder(scriptSystem.getVm(), "core/Position", "Position");
    binder.property<&Position::x>("x");
    binder.property<&Position::y>("y");
    std::string wrenPositionClass = binder.generateWrenClass();
    EXPECT_TRUE(scriptSystem.getVm().interpret("core/Position", wrenPositionClass));

    const char* scriptCode =
        "import \"core/Entity\" for Entity\n"
        "import \"core/Position\" for Position\n"
        "import \"core/Input\" for Input\n"
        "class InputTestScript {\n"
        "    construct new(entity) {\n"
        "        _entity = entity\n"
        "        _pos = Position.new(entity.id)\n"
        "    }\n"
        "    pollInput() {\n"
        "        if (Input.isDown(0, \"shoot\")) {\n"
        "            _pos.x = 100.0\n"
        "        }\n"
        "        _pos.y = Input.getAxis(0, \"move_y\")\n"
        "    }\n"
        "}\n";

    EXPECT_TRUE(scriptSystem.getVm().interpret("game", scriptCode));

    ecs::Entity const e1 = registry.spawnEntity();
    registry.addComponent<Position>(e1, Position{.x = 0.0F, .y = 0.0F});
    registry.addComponent<ecs::ScriptComponent>(e1, ecs::ScriptComponent{.moduleName = "game",
                                                                         .className = "InputTestScript",
                                                                         .instanceHandle = nullptr,
                                                                         .hasUpdate = false,
                                                                         .initialized = false});

    scriptSystem.update(0.016F);

    auto& script = registry.getComponents<ecs::ScriptComponent>().get(e1);
    EXPECT_TRUE(script.initialized);
    EXPECT_NE(script.instanceHandle, nullptr);

    auto pollHandle = scriptSystem.getVm().makeCallHandle("pollInput()");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 1);
    wrenSetSlotHandle(scriptSystem.getVm().raw(), 0, static_cast<WrenHandle*>(script.instanceHandle));
    EXPECT_EQ(scriptSystem.getVm().call(pollHandle), WREN_RESULT_SUCCESS);

    auto& pos = registry.getComponents<Position>().get(e1);
    EXPECT_FLOAT_EQ(pos.x, 100.0F);
    EXPECT_FLOAT_EQ(pos.y, -0.75F);

    // Direct static method query with player index
    auto isJustPressedHandle = scriptSystem.getVm().makeCallHandle("isJustPressed(_,_)");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 3);
    wrenGetVariable(scriptSystem.getVm().raw(), "core/Input", "Input", 0);
    wrenSetSlotDouble(scriptSystem.getVm().raw(), 1, 0.0);
    wrenSetSlotString(scriptSystem.getVm().raw(), 2, "shoot");
    EXPECT_EQ(scriptSystem.getVm().call(isJustPressedHandle), WREN_RESULT_SUCCESS);
    EXPECT_TRUE(wrenGetSlotBool(scriptSystem.getVm().raw(), 0));

    // Default wrapper query without player index
    auto defaultIsJustPressedHandle = scriptSystem.getVm().makeCallHandle("isJustPressed(_)");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 2);
    wrenGetVariable(scriptSystem.getVm().raw(), "core/Input", "Input", 0);
    wrenSetSlotString(scriptSystem.getVm().raw(), 1, "shoot");
    EXPECT_EQ(scriptSystem.getVm().call(defaultIsJustPressedHandle), WREN_RESULT_SUCCESS);
    EXPECT_TRUE(wrenGetSlotBool(scriptSystem.getVm().raw(), 0));

    auto isJustReleasedHandle = scriptSystem.getVm().makeCallHandle("isJustReleased(_,_)");
    wrenEnsureSlots(scriptSystem.getVm().raw(), 3);
    wrenGetVariable(scriptSystem.getVm().raw(), "core/Input", "Input", 0);
    wrenSetSlotDouble(scriptSystem.getVm().raw(), 1, 0.0);
    wrenSetSlotString(scriptSystem.getVm().raw(), 2, "shoot");
    EXPECT_EQ(scriptSystem.getVm().call(isJustReleasedHandle), WREN_RESULT_SUCCESS);
    EXPECT_FALSE(wrenGetSlotBool(scriptSystem.getVm().raw(), 0));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
