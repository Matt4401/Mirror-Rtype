#include <gtest/gtest.h>

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Engine.hpp"
#include "ecs/Entity.hpp"
#include "ecs/Registry.hpp"
#include "ecs/time/Time.hpp"
#include "hierarchy/Hierarchy.hpp"
#include "hierarchy/HierarchyPlugin.hpp"
#include "scene/IScene.hpp"
#include "scene/SceneContext.hpp"
#include "scene/SceneEvents.hpp"
#include "scene/SceneManager.hpp"
#include "scene/ScenePlugin.hpp"
#include "scene/SceneTypes.hpp"

namespace {

using scene::SceneContext;
using scene::SceneId;
using scene::SceneManager;
using scene::SceneMode;
using scene::SceneStage;

/** @brief Event used to check scene subscriptions. */
struct Ping {
    int value = 0;
};

/** @brief Scene recording its hooks in a shared log, with a configurable mode. */
class RecordingScene : public scene::IScene {
  public:
    RecordingScene(std::string name, std::vector<std::string>& log, SceneMode mode = SceneMode::Exclusive,
                   bool pausesBelow = false)
        : _name(std::move(name)), _log(log), _mode(mode), _pausesBelow(pausesBelow) {}

    [[nodiscard]] std::string_view name() const noexcept override { return _name; }
    [[nodiscard]] SceneMode mode() const noexcept override { return _mode; }
    [[nodiscard]] bool pausesBelow() const noexcept override { return _pausesBelow; }

    void onEnter(SceneContext& context) override {
        _log.push_back(_name + ".enter");
        if (onEnterHook) {
            onEnterHook(context);
        }
    }
    void onExit(SceneContext& context) override {
        _log.push_back(_name + ".exit");
        if (onExitHook) {
            onExitHook(context);
        }
    }
    void onPause(SceneContext& /*context*/) override { _log.push_back(_name + ".pause"); }
    void onResume(SceneContext& /*context*/) override { _log.push_back(_name + ".resume"); }

    std::function<void(SceneContext&)> onEnterHook;
    std::function<void(SceneContext&)> onExitHook;

  private:
    std::string _name;
    std::vector<std::string>& _log;
    SceneMode _mode;
    bool _pausesBelow;
};

/** @brief Distinct scene types, since a manager holds one scene per type. */
class MenuScene final : public RecordingScene {
  public:
    explicit MenuScene(std::vector<std::string>& log) : RecordingScene("Menu", log) {}
};
class GameScene final : public RecordingScene {
  public:
    explicit GameScene(std::vector<std::string>& log) : RecordingScene("Game", log) {}
};
class PauseScene final : public RecordingScene {
  public:
    explicit PauseScene(std::vector<std::string>& log) : RecordingScene("Pause", log, SceneMode::Overlay) {}
};
class ModalScene final : public RecordingScene {
  public:
    explicit ModalScene(std::vector<std::string>& log) : RecordingScene("Modal", log, SceneMode::Overlay, true) {}
};

/** @brief Engine with a ScenePlugin and the four test scenes. */
class SceneTest : public ::testing::Test {
  protected:
    void SetUp() override {
        engine.addPlugin<scene::ScenePlugin>();
        engine.addPlugin<hierarchy::HierarchyPlugin>();
        SceneManager& scenes = manager();
        menu = scenes.registerScene<MenuScene>(log);
        game = scenes.registerScene<GameScene>(log);
        pause = scenes.registerScene<PauseScene>(log);
        modal = scenes.registerScene<ModalScene>(log);
        engine.init();
    }

    SceneManager& manager() { return engine.getPlugin<scene::ScenePlugin>().manager(); }

    void frame() { (void)engine.advance(); }

    // Declared first so it outlives the engine, whose shutdown still runs the hooks writing to it.
    std::vector<std::string> log;
    engine::Engine engine;
    SceneId menu = scene::INVALID_SCENE;
    SceneId game = scene::INVALID_SCENE;
    SceneId pause = scene::INVALID_SCENE;
    SceneId modal = scene::INVALID_SCENE;
};

TEST_F(SceneTest, ScenesAreRegisteredWithDenseIdsAndFoundByNameOrType) {
    EXPECT_EQ(menu, 0);
    EXPECT_EQ(game, 1);
    EXPECT_EQ(manager().count(), 4U);
    EXPECT_EQ(manager().find("Pause"), pause);
    EXPECT_FALSE(manager().find("Nope").has_value());
    EXPECT_EQ(manager().idOf<GameScene>(), game);
    EXPECT_EQ(manager().get<GameScene>().name(), "Game");
    EXPECT_THROW((void)manager().registerScene<MenuScene>(log), std::invalid_argument);
    EXPECT_THROW(manager().push(static_cast<SceneId>(99)), std::out_of_range);
}

TEST_F(SceneTest, ManagerIsAvailableInTheMediator) { EXPECT_EQ(&engine.service<SceneManager>(), &manager()); }

TEST_F(SceneTest, TransitionsWaitForTheNextFrame) {
    manager().switchTo(menu);
    EXPECT_EQ(manager().top(), scene::INVALID_SCENE);
    EXPECT_TRUE(manager().hasPendingTransitions());

    frame();
    EXPECT_EQ(manager().top(), menu);
    EXPECT_TRUE(manager().isActive(menu));
    EXPECT_TRUE(manager().isRunning(menu));
    EXPECT_TRUE(manager().isVisible(menu));
    EXPECT_EQ(log, (std::vector<std::string>{"Menu.enter"}));
}

TEST_F(SceneTest, ExclusiveScenePausesAndHidesTheSceneBelowBeforeEntering) {
    manager().switchTo(menu);
    frame();
    manager().push(game);
    frame();

    EXPECT_EQ(log, (std::vector<std::string>{"Menu.enter", "Menu.pause", "Game.enter"}));
    EXPECT_FALSE(manager().isRunning(menu));
    EXPECT_FALSE(manager().isVisible(menu));
    EXPECT_TRUE(manager().isRunning(game));

    manager().pop();
    frame();
    EXPECT_EQ(log.back(), "Menu.resume");
    EXPECT_TRUE(manager().isRunning(menu));
    EXPECT_TRUE(manager().isVisible(menu));
    EXPECT_FALSE(manager().isActive(game));
}

TEST_F(SceneTest, OverlayKeepsTheSceneBelowRunningAndVisible) {
    manager().switchTo(game);
    manager().push(pause);
    frame();

    EXPECT_TRUE(manager().isRunning(game));
    EXPECT_TRUE(manager().isVisible(game));
    EXPECT_TRUE(manager().isRunning(pause));
    EXPECT_EQ(log, (std::vector<std::string>{"Game.enter", "Pause.enter"}));
}

TEST_F(SceneTest, PausingOverlayStopsTheSceneBelowButKeepsItVisible) {
    manager().switchTo(game);
    manager().push(modal);
    frame();

    EXPECT_FALSE(manager().isRunning(game));
    EXPECT_TRUE(manager().isVisible(game));
}

TEST_F(SceneTest, SwitchExitsEveryScenesTopFirstWithoutResumingThem) {
    manager().switchTo(menu);
    manager().push(game);
    manager().push(pause);
    frame();
    log.clear();

    manager().switchTo(menu);
    frame();
    EXPECT_EQ(log, (std::vector<std::string>{"Pause.exit", "Game.exit", "Menu.exit", "Menu.enter"}));
    EXPECT_EQ(manager().stack().size(), 1U);
}

TEST_F(SceneTest, ReplaceSwapsOnlyTheTopScene) {
    manager().switchTo(menu);
    manager().push(pause);
    frame();
    log.clear();

    manager().replace(game);
    frame();
    EXPECT_EQ(log, (std::vector<std::string>{"Pause.exit", "Menu.pause", "Game.enter"}));
    ASSERT_EQ(manager().stack().size(), 2U);
    EXPECT_EQ(manager().stack()[0], menu);
    EXPECT_EQ(manager().top(), game);
}

TEST_F(SceneTest, PopOnAnEmptyStackDoesNothing) {
    manager().pop();
    EXPECT_NO_THROW(frame());
    EXPECT_EQ(manager().top(), scene::INVALID_SCENE);
}

TEST_F(SceneTest, PushingASceneAlreadyOnTheStackThrows) {
    manager().switchTo(menu);
    frame();
    manager().push(menu);
    EXPECT_THROW(frame(), std::logic_error);
    EXPECT_FALSE(manager().hasPendingTransitions());
}

TEST_F(SceneTest, TransitionQueuedByAHookIsAppliedInTheSameFrame) {
    manager().get<MenuScene>().onEnterHook = [this](SceneContext& /*context*/) { manager().push(pause); };
    manager().switchTo(menu);
    frame();
    EXPECT_EQ(manager().top(), pause);
}

TEST_F(SceneTest, ScenesSwitchingEachOtherForeverAreStopped) {
    manager().get<MenuScene>().onEnterHook = [this](SceneContext& /*context*/) { manager().switchTo(game); };
    manager().get<GameScene>().onEnterHook = [this](SceneContext& /*context*/) { manager().switchTo(menu); };
    manager().switchTo(menu);
    EXPECT_THROW(frame(), std::logic_error);
}

TEST_F(SceneTest, ExitDestroysOwnedEntitiesAndTheirChildrenButNotOthers) {
    ecs::Entity owned = ecs::NULL_ENTITY;
    ecs::Entity adopted = ecs::NULL_ENTITY;
    ecs::Entity child = ecs::NULL_ENTITY;
    ecs::Registry& registry = engine.registry();
    const ecs::Entity persistent = registry.spawnEntity();
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) {
        owned = context.spawn();
        adopted = registry.spawnEntity();
        context.adopt(adopted);
        child = registry.spawnEntity();
        hierarchy::setParent(registry, child, owned);
    };
    manager().switchTo(game);
    frame();
    EXPECT_TRUE(registry.isAlive(owned));
    EXPECT_EQ(registry.getComponents<scene::SceneMember>().get(owned).id, game);

    manager().switchTo(menu);
    frame();
    EXPECT_FALSE(registry.isAlive(owned));
    EXPECT_FALSE(registry.isAlive(adopted));
    EXPECT_FALSE(registry.isAlive(child));
    EXPECT_TRUE(registry.isAlive(persistent));
}

TEST_F(SceneTest, ExitKeepsTheEntitiesOfOtherScenes) {
    ecs::Entity menuEntity = ecs::NULL_ENTITY;
    manager().get<MenuScene>().onEnterHook = [&](SceneContext& context) { menuEntity = context.spawn(); };
    manager().switchTo(menu);
    manager().push(pause);
    frame();
    manager().pop();
    frame();
    EXPECT_TRUE(engine.registry().isAlive(menuEntity));
}

TEST_F(SceneTest, SubscriptionsAreCancelledOnExit) {
    int received = 0;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) {
        context.subscribe<Ping>([&](const Ping& ping) { received += ping.value; });
    };
    manager().switchTo(game);
    frame();
    engine.eventBus().publish(Ping{.value = 2});
    EXPECT_EQ(received, 2);

    manager().pop();
    frame();
    engine.eventBus().publish(Ping{.value = 5});
    EXPECT_EQ(received, 2);

    manager().switchTo(game);
    frame();
    engine.eventBus().publish(Ping{.value = 1});
    EXPECT_EQ(received, 3);
}

TEST_F(SceneTest, SystemsRunOnlyWhileTheirSceneRuns) {
    int updates = 0;
    int lateUpdates = 0;
    manager().get<MenuScene>().onEnterHook = [&](SceneContext& context) {
        context.addSystem(SceneStage::Update, [&](engine::Engine& /*e*/, const ecs::time::Time& /*t*/) { ++updates; });
        context.addSystem(SceneStage::LateUpdate,
                          [&](engine::Engine& /*e*/, const ecs::time::Time& /*t*/) { ++lateUpdates; });
    };
    manager().switchTo(menu);
    frame();
    frame();
    EXPECT_EQ(updates, 2);
    EXPECT_EQ(lateUpdates, 2);

    manager().push(game);
    frame();
    EXPECT_EQ(updates, 2);

    manager().pop();
    frame();
    EXPECT_EQ(updates, 3);

    manager().switchTo(game);
    frame();
    frame();
    EXPECT_EQ(updates, 3);
}

TEST_F(SceneTest, SystemAddedWhileSystemsRunStartsAtTheNextRun) {
    int added = 0;
    bool once = false;
    manager().get<MenuScene>().onEnterHook = [&](SceneContext& context) {
        context.addSystem(SceneStage::Update, [&](engine::Engine& /*e*/, const ecs::time::Time& /*t*/) {
            if (!once) {
                once = true;
                context.addSystem(SceneStage::Update,
                                  [&](engine::Engine& /*e*/, const ecs::time::Time& /*t*/) { ++added; });
            }
        });
    };
    manager().switchTo(menu);
    frame();
    EXPECT_EQ(added, 0);
    frame();
    EXPECT_EQ(added, 1);
}

TEST_F(SceneTest, SceneEventsArePublished) {
    std::vector<std::string> events;
    auto& bus = engine.eventBus();
    (void)bus.subscribe<scene::SceneEntered>(
        [&](const scene::SceneEntered& e) { events.push_back("entered " + std::to_string(e.id)); });
    (void)bus.subscribe<scene::SceneExited>(
        [&](const scene::SceneExited& e) { events.push_back("exited " + std::to_string(e.id)); });
    (void)bus.subscribe<scene::ScenePaused>(
        [&](const scene::ScenePaused& e) { events.push_back("paused " + std::to_string(e.id)); });
    (void)bus.subscribe<scene::SceneResumed>(
        [&](const scene::SceneResumed& e) { events.push_back("resumed " + std::to_string(e.id)); });
    (void)bus.subscribe<scene::SceneVisibilityChanged>([&](const scene::SceneVisibilityChanged& e) {
        events.push_back((e.visible ? "shown " : "hidden ") + std::to_string(e.id));
    });

    manager().switchTo(menu);
    manager().push(game);
    manager().pop();
    frame();
    EXPECT_EQ(events, (std::vector<std::string>{"entered 0", "hidden 0", "paused 0", "entered 1", "exited 1", "shown 0",
                                                "resumed 0"}));
    // The listeners capture `events`: shut down while it is alive, since shutdown still publishes `SceneExited`.
    engine.shutdown();
}

TEST_F(SceneTest, ShutdownExitsEveryScene) {
    ecs::Entity owned = ecs::NULL_ENTITY;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) { owned = context.spawn(); };
    manager().switchTo(menu);
    manager().push(game);
    frame();
    log.clear();

    engine.shutdown();
    EXPECT_EQ(log, (std::vector<std::string>{"Game.exit", "Menu.exit"}));
    EXPECT_FALSE(engine.registry().isAlive(owned));
    EXPECT_FALSE(manager().isActive(menu));
}

TEST_F(SceneTest, FailedEntryIsRolledBack) {
    ecs::Entity spawned = ecs::NULL_ENTITY;
    int pings = 0;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) {
        spawned = context.spawn();
        context.subscribe<Ping>([&pings](const Ping& /*ping*/) { ++pings; });
        throw std::runtime_error("enter");
    };
    manager().switchTo(menu);
    frame();
    log.clear();

    manager().push(game);
    EXPECT_THROW(manager().applyTransitions(), std::runtime_error);
    EXPECT_EQ(log, (std::vector<std::string>{"Menu.pause", "Game.enter", "Menu.resume"}));
    EXPECT_FALSE(manager().isActive(game));
    EXPECT_EQ(manager().top(), menu);
    EXPECT_TRUE(manager().isRunning(menu));
    EXPECT_TRUE(manager().isVisible(menu));
    EXPECT_FALSE(engine.registry().isAlive(spawned));
    engine.eventBus().publish(Ping{});
    EXPECT_EQ(pings, 0);
}

TEST_F(SceneTest, ThrowingExitHookStillRemovesTheScene) {
    ecs::Entity owned = ecs::NULL_ENTITY;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) { owned = context.spawn(); };
    manager().get<GameScene>().onExitHook = [](SceneContext& /*context*/) { throw std::runtime_error("exit"); };
    manager().switchTo(menu);
    manager().push(game);
    frame();

    manager().pop();
    EXPECT_THROW(manager().applyTransitions(), std::runtime_error);
    EXPECT_FALSE(manager().isActive(game));
    EXPECT_FALSE(engine.registry().isAlive(owned));
    EXPECT_EQ(manager().top(), menu);
    EXPECT_TRUE(manager().isRunning(menu));
}

TEST_F(SceneTest, WhatDestroyListenersAddToAnExitingSceneIsReleasedToo) {
    SceneContext& context = manager().context(game);
    ecs::Entity owned = ecs::NULL_ENTITY;
    ecs::Entity late = ecs::NULL_ENTITY;
    int pings = 0;
    int runs = 0;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& ctx) { owned = ctx.spawn(); };
    engine.registry().addDestroyListener([&](ecs::Entity entity) {
        if (entity == owned) {
            late = context.spawn();
            context.subscribe<Ping>([&pings](const Ping& /*ping*/) { ++pings; });
            context.addSystem(SceneStage::Update,
                              [&runs](engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) { ++runs; });
        }
    });
    manager().switchTo(game);
    frame();

    manager().switchTo(menu);
    frame();
    EXPECT_FALSE(engine.registry().isAlive(owned));
    EXPECT_FALSE(engine.registry().isAlive(late));
    engine.eventBus().publish(Ping{});
    EXPECT_EQ(pings, 0);

    manager().get<GameScene>().onEnterHook = nullptr;
    manager().switchTo(game);
    frame();
    EXPECT_EQ(runs, 0);
}

TEST_F(SceneTest, ExitDestroysLongChainsOfEntitiesSpawnedDuringTheExit) {
    // Each destroyed member spawns the next one into the exiting scene, deeper than any fixed number of passes.
    constexpr int CHAIN = 20;
    SceneContext& context = manager().context(game);
    std::vector<ecs::Entity> chain;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& ctx) { chain.push_back(ctx.spawn()); };
    engine.registry().addDestroyListener([&](ecs::Entity entity) {
        if (!chain.empty() && entity == chain.back() && chain.size() < CHAIN) {
            chain.push_back(context.spawn());
        }
    });
    manager().switchTo(game);
    frame();

    manager().switchTo(menu);
    EXPECT_NO_THROW(manager().applyTransitions());
    ASSERT_EQ(chain.size(), static_cast<std::size_t>(CHAIN));
    for (const ecs::Entity entity : chain) {
        EXPECT_FALSE(engine.registry().isAlive(entity));
    }
}

TEST(SceneShutdown, ThrowingDestroyListenerStillDetachesTheManager) {
    std::vector<std::string> log;
    engine::Engine engine;
    engine.addPlugin<scene::ScenePlugin>();
    SceneManager& manager = engine.getPlugin<scene::ScenePlugin>().manager();
    const SceneId game = manager.registerScene<GameScene>(log);
    ecs::Entity owned = ecs::NULL_ENTITY;
    manager.get<GameScene>().onEnterHook = [&](SceneContext& context) { owned = context.spawn(); };
    engine.init();
    engine.registry().addDestroyListener([&owned](ecs::Entity entity) {
        if (entity == owned) {
            throw std::runtime_error("destroy");
        }
    });
    manager.switchTo(game);
    (void)engine.advance();

    EXPECT_THROW(engine.shutdown(), std::runtime_error);
    EXPECT_FALSE(engine.registry().isAlive(owned));
    EXPECT_FALSE(manager.isActive(game));
    EXPECT_EQ(manager.engine(), nullptr);
    EXPECT_FALSE(engine.mediator().getService<SceneManager>().has_value());
}

TEST_F(SceneTest, SceneSystemsCannotApplyOrClearTheStack) {
    bool cleared = false;
    manager().get<GameScene>().onEnterHook = [&](SceneContext& context) {
        context.addSystem(SceneStage::Update, [&](engine::Engine& /*engine*/, const ecs::time::Time& /*time*/) {
            manager().switchTo(menu);
            manager().applyTransitions();  // deferred: the system keeps running safely
            EXPECT_EQ(manager().top(), game);
            try {
                manager().clear();
            } catch (const std::logic_error&) {
                cleared = true;
            }
        });
    };
    manager().switchTo(game);
    frame();  // enters the game, whose system runs in the same frame
    EXPECT_TRUE(cleared);
    EXPECT_EQ(manager().top(), game);
    frame();  // the switch queued by the system is applied at the frame boundary
    EXPECT_EQ(manager().top(), menu);
}

TEST_F(SceneTest, ClearDropsTransitionsQueuedByExitHooks) {
    manager().get<GameScene>().onExitHook = [this](SceneContext& /*context*/) { manager().switchTo(menu); };
    manager().switchTo(game);
    frame();

    manager().clear();
    EXPECT_EQ(manager().top(), scene::INVALID_SCENE);
    EXPECT_FALSE(manager().hasPendingTransitions());
}

TEST(SceneShutdown, ScenesExitBeforeTheHierarchyHooksGoEvenWhenAddedFirst) {
    std::vector<std::string> log;
    engine::Engine engine;
    engine.addPlugin<scene::ScenePlugin>();
    engine.addPlugin<hierarchy::HierarchyPlugin>();
    SceneManager& manager = engine.getPlugin<scene::ScenePlugin>().manager();
    const SceneId game = manager.registerScene<GameScene>(log);
    ecs::Entity child = ecs::NULL_ENTITY;
    manager.get<GameScene>().onEnterHook = [&](SceneContext& context) {
        const ecs::Entity parent = context.spawn();
        child = context.registry().spawnEntity();  // not owned: only the hierarchy cascade destroys it
        hierarchy::setParent(context.registry(), child, parent);
    };
    engine.init();
    manager.switchTo(game);
    (void)engine.advance();
    ASSERT_TRUE(engine.registry().isAlive(child));

    engine.shutdown();
    EXPECT_FALSE(engine.registry().isAlive(child));
}

TEST(SceneManagerStandalone, TransitionsBeforeAttachWaitForTheEngine) {
    SceneManager manager;
    std::vector<std::string> log;
    const SceneId menu = manager.registerScene<MenuScene>(log);
    manager.switchTo(menu);
    manager.applyTransitions();
    EXPECT_EQ(manager.top(), scene::INVALID_SCENE);
    EXPECT_TRUE(manager.hasPendingTransitions());
    EXPECT_THROW((void)manager.context(menu).engine(), std::logic_error);
}

}  // namespace

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
