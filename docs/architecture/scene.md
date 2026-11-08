# Scenes

The `engine_scene` module splits a game into **scenes**: a title screen, a lobby, a level, a pause overlay. A scene
owns everything it creates (entities, event subscriptions, systems) and releases all of it when it leaves the stack,
so switching from the menu to a level and back never leaks an entity or a listener.

It is generic and headless: it depends only on `engine_core` and `engine_events`, never on rendering, networking or
scripting, so a server can use it too.

## Concepts

| Type                | Header                    | Role                                                                     |
|---------------------|---------------------------|--------------------------------------------------------------------------|
| `IScene`            | `scene/IScene.hpp`        | A scene: optional hooks `onEnter`, `onExit`, `onPause`, `onResume`.      |
| `SceneContext`      | `scene/SceneContext.hpp`  | What the scene uses to create what it owns.                              |
| `SceneManager`      | `scene/SceneManager.hpp`  | Registered scenes, the stack and the queued transitions.                 |
| `ScenePlugin`       | `scene/ScenePlugin.hpp`   | Applies transitions and runs scene systems from the engine loop.         |
| `SceneMember`       | `scene/SceneTypes.hpp`    | Component marking an entity as owned by a scene.                         |
| Events              | `scene/SceneEvents.hpp`   | `SceneEntered`, `SceneExited`, `ScenePaused`, `SceneResumed`, `SceneVisibilityChanged`. |

The engine keeps **one registry**. A scene does not own a world of its own: its entities carry a 2-byte
`SceneMember{id}` component. Entities without it (a camera, an input state, replicated entities managed by the network
module) outlive scene changes.

## Writing a scene

```cpp
#include "scene/IScene.hpp"
#include "scene/SceneContext.hpp"

class TitleScene final : public scene::IScene {
  public:
    [[nodiscard]] std::string_view name() const noexcept override { return "Title"; }

    void onEnter(scene::SceneContext& context) override {
        const ecs::Entity logo = context.spawn();  // destroyed when the scene exits
        context.registry().addComponent(logo, ui::UiText{.text = "MY GAME"});

        context.subscribe<ui::UiButtonClickedEvent>([&context](const ui::UiButtonClickedEvent& click) {
            if (click.action == "play") {
                context.manager().switchTo<GameScene>();
            }
        });

        context.addSystem(scene::SceneStage::Update, [](engine::Engine& engine, const ecs::time::Time& time) {
            // runs every frame while the title runs
        });
    }
};
```

| `SceneContext` call               | Released on exit by                                                        |
|-----------------------------------|-----------------------------------------------------------------------------|
| `spawn()`                         | destroying every entity whose `SceneMember` names the scene               |
| `adopt(entity)`                   | same: the entity gets the scene's `SceneMember`                            |
| `subscribe<E>(callback)`          | cancelling the subscription (stored as a token and a typed function pointer) |
| `addSystem(stage, system)`        | dropping the system                                                        |

Children of an owned entity (`hierarchy::setParent`) are destroyed with it when `HierarchyPlugin` is loaded, so a
UI panel spawned with `spawn()` takes its widgets with it. `ScenePlugin` shuts down before `HierarchyPlugin`, in
whatever order they were added, so this also holds when the program closes.

What a destroy listener adds to the exiting scene while its entities go (an entity, a subscription, a system) is
released as well.

Errors in the hooks never leave a half-entered or half-exited scene:

- If `onEnter` throws, the scene is rolled back: its entities, subscriptions and systems go, it leaves the stack, and
  the scenes below run and show again. It gets no `onExit` and no `SceneExited`.
- If `onExit` or a destroy listener throws, the scene still leaves the stack completely.

In both cases the first error is then rethrown.

Subscriptions also deliver events while the scene is paused; check `context.isRunning()` in the callback when that
matters. Systems only run while the scene runs.

## Registering and switching

```cpp
engine.addPlugin<scene::ScenePlugin>();
scene::SceneManager& scenes = engine.getPlugin<scene::ScenePlugin>().manager();
scenes.registerScene<TitleScene>();
scenes.registerScene<GameScene>(levelConfig);  // constructor arguments
scenes.switchTo<TitleScene>();                 // applied on the first frame
engine.run();
```

Anywhere else, the manager is a Mediator service: `engine.service<scene::SceneManager>()`.

A scene is registered once (one per type, unique name) and keeps its state between visits.
`registerScene` returns a dense `SceneId`; `find(name)` and `idOf<T>()` look it up.

| Call            | Effect                                                    |
|-----------------|-----------------------------------------------------------|
| `switchTo(id)`  | Exits every scene of the stack, top first, then enters `id`. |
| `push(id)`      | Enters `id` on top of the stack.                          |
| `pop()`         | Exits the top scene (nothing on an empty stack).          |
| `replace(id)`   | Exits the top scene, then enters `id` in its place.       |

!!! note "Transitions are deferred"
    These calls only queue the change. `ScenePlugin::preUpdate` applies the queue at the start of the next frame,
    so no entity disappears in the middle of a system and every stage of a frame sees the same stack.
    A transition queued by a hook (`onEnter` pushing an overlay, for instance) is applied in the same frame.
    More than `SceneManager::MAX_TRANSITIONS_PER_FRAME` (64) transitions in one frame throws `std::logic_error`:
    scenes are switching each other forever. Pushing a scene already on the stack also throws.
    From a scene system, `applyTransitions()` does nothing (the queue waits for the next frame) and `clear()` throws,
    since both would destroy the system while it runs. `clear()` also drops what the exit hooks queue.

## Stack, modes and pausing

The top scene always runs and is visible. Below it, each scene's `mode()` decides:

| Scene above                      | Scenes below are       |
|----------------------------------|------------------------|
| `SceneMode::Exclusive` (default) | hidden and paused      |
| `SceneMode::Overlay`             | visible and running    |
| `Overlay` with `pausesBelow()`   | visible but paused     |

A local pause menu in a networked game is an `Overlay` that does not pause below: the game keeps simulating and
replicating behind it. A blocking dialog is an `Overlay` that pauses below.

Hook order on a push: the scenes below get `onPause` (and `SceneVisibilityChanged`) **before** the new scene's
`onEnter`. On a pop, the exiting scene gets `onExit`, its entities are destroyed, then the scenes below get `onResume`.
`switchTo` exits the whole stack without resuming anyone.

Plugins that are not scenes can gate themselves on the stack: `isActive(id)` (on the stack), `isRunning(id)`,
`isVisible(id)` and `top()` are flag lookups, cheap enough for every frame. They can also listen to the scene events.

## Frame timeline

```text
preUpdate     ScenePlugin applies queued transitions (hooks, entity cleanup, events)
fixedUpdate   systems added with SceneStage::FixedUpdate, of running scenes, bottom first
update        SceneStage::Update systems
lateUpdate    SceneStage::LateUpdate systems
shutdown      every scene exits, top first
```

A system added while systems run (from a system, or from an event published by one) starts at the next run of its
stage, so a system list never changes under its own loop.

## Performance

- **No allocation per frame.** Systems are stored once per scene; the subscription list, the system lists, the
  transition queue and the scratch buffer used to collect a scene's entities keep their capacity between visits.
- **Unloading a scene** is one pass over the dense `SceneMember` array, then one `Registry::killEntities` call. The
  batch runs the same destroy listeners as `killEntity` (scripts, hierarchy, network map) but does the listener
  bookkeeping once for the whole batch.
- **Queries** are flag lookups by dense id; names are only resolved by `find`, never per frame.
