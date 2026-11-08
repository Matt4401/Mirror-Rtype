# Assets

The `engine_assets` module is the engine's asset manager. Games name their textures, fonts, sounds and musics by
**alias** (`"player_ship"`) instead of by file, and gather them into **groups**: `"global"` for what the whole game
uses, one group per scene or level for the rest. A file is loaded **once** per set of load options, however many times
it is declared or requested, and is released when the last group holding it is unloaded.

The module is generic and headless. It depends only on `engine_core` and never on raylib. Small bridge modules connect
it to the rest of the engine, so that neither side knows the other:

| Module                    | Depends on                         | Role                                                       |
|---------------------------|------------------------------------|------------------------------------------------------------|
| `engine_assets`           | `engine_core`                      | `AssetManager`, `AssetPlugin`; headless.                   |
| `engine_assets_scripting` | `engine_assets`, `engine_scripting`| Wren module `core/Assets`, the game's declaration script.  |
| `engine_assets_scene`     | `engine_assets`, `engine_scene`    | A scene's asset group is loaded while it is on the stack.  |
| `engine_assets_render`    | `engine_assets`, `engine_render`   | Texture and font loaders (graphics builds only).           |
| `engine_assets_audio`     | `engine_assets`, `engine_audio`    | Sound and music loaders (graphics builds only).            |

## Why a manager

Assets were never entities: an entity only carries a handle (`Renderable::textureId`, `AudioEmitter::sound`), and the
renderer resolves it in O(1). The manager changes nothing per frame. It fixes what happens around loading:

| Before                                                      | With `engine_assets`                                          |
|-------------------------------------------------------------|---------------------------------------------------------------|
| Hard-coded paths in each game plugin                        | Aliases declared in one Wren script per game                  |
| Prefabs took a raw `textureId` number                       | Prefabs take a `texture` alias                                |
| Loading a file twice took twice the memory                  | One load per normalised path (`a/../a/x.png` is `a/x.png`)    |
| `TextureStore` and `FontStore` could not release one asset  | Groups release what no other group holds                      |
| Three APIs and three ways to fail (exception, log, silence) | One API, one status per asset (`AssetStatus`)                 |

## Concepts

| Type                   | Header                                     | Role                                                        |
|------------------------|--------------------------------------------|-------------------------------------------------------------|
| `AssetManager`         | `assets/AssetManager.hpp`                  | Registry resource: kinds, declarations, groups, handles.    |
| `Texture`, `Font`, `Sound`, `Music` | `assets/AssetKinds.hpp`       | The engine's kinds (`Kind::Id`, `Kind::NONE`, `Kind::NAME`).|
| `AssetKind`            | `assets/AssetKinds.hpp`                    | Concept a game's own kind satisfies.                        |
| `AssetLoader<Kind>`    | `assets/AssetKinds.hpp`                    | `load` / `unload` (+ optional `fallback`, `reserve`).       |
| `AssetStatus`          | `assets/AssetKinds.hpp`                    | `Unknown`, `Declared`, `Loaded`, `Failed`.                  |
| `GroupProgress`        | `assets/AssetKinds.hpp`                    | Loaded / failed / waiting counts of a group.                |
| `AssetPlugin`          | `assets/AssetPlugin.hpp`                   | Adds the manager to the registry, releases it at shutdown.  |

Handles are the engine's existing types (`ecs::TextureId`, `engine::FontId`, `audio::SoundId`, `audio::MusicId`, the
last two in `core/AudioHandles.hpp`): components, the render system and the audio system are unchanged.

### Kinds are open

`AssetManager` stores one cache per kind behind a common interface. The four engine kinds are registered by its
constructor; any other type satisfying `AssetKind` is registered on first use, so a game adds its own kinds (levels,
shaders, atlases...) without touching the engine:

```cpp
struct Level {
    using Id = std::uint32_t;
    static constexpr Id NONE = 0;
    static constexpr std::string_view NAME = "level";
    static constexpr std::array<std::string_view, 1> EXTENSIONS = {".json"};
};

manager.registerKind<Level>();                          // so scripts can name it before C++ uses it
manager.setLoader<Level>(AssetLoader<Level>{.load = ..., .unload = ...});
manager.declare<Level>("stage1", "levels/stage1.json", "global");
```

Scripts reach every kind by its `NAME`: `Assets.declare("level", "stage1", "levels/stage1.json")`.

### Stable handles

An asset loaded again (its group was unloaded then loaded, or its backend restarted) passes the handle it had to the
loader as `preferred`. The engine's stores (`TextureStore`, `FontStore`, `RaylibAudioBackend`) give it back when it is
free, so a handle kept by a component or by C++ code names the asset again once it is back. While the asset is
unloaded the handle resolves to nothing; a fresh load never takes a released slot, so a handle is never given to
another asset. When a loader is installed, the manager first calls `reserve` with the highest handle it knows, so the
new backend keeps those handles free for the assets that will ask for them.

## Lifecycle

```text
AssetPlugin::init           the AssetManager resource is added (with the engine's four kinds)
AssetScriptingPlugin::init  the declaration script runs: aliases and groups are recorded
RenderAssetsPlugin /        loaders are plugged in (after RenderPlugin / AudioPlugin): what a group already asked
  AudioAssetsPlugin::init     for is loaded now
game plugins / scenes       loadGroup("global"), get<Texture>("player_ship"); scenes load their own group
RenderAssetsPlugin /        detachLoader releases their assets, before RenderPlugin / AudioPlugin close the window and
  AudioAssetsPlugin::shutdown the device
AssetPlugin::shutdown       unloadAll, then the resource is removed
```

The declaration script and the loaders may come in either order: requests made before a loader exists are served as
soon as it is installed. A game plugin that resolves handles in its `init` requires the bridge
(`builder.require<assets::RenderAssetsPlugin>()`) so the loader is there.

With a muted audio backend (no device, `NullAudioBackend`), `AudioAssetsPlugin` installs no loader: sounds stay
`Declared` instead of failing one by one.

## Declaring assets in Wren

Each game has one declaration module, given to `AssetScriptingPlugin`:

```wren
import "core/Assets" for Assets

Assets.texture("player_ship", "sprites/r-typesheet42.gif")          // group "global"
Assets.smoothTexture("stadium", "rlss/assets/textures/stadium.png")   // mipmapped, trilinear
Assets.sound("boss_roar", "sounds/boss.wav", "Level3")
Assets.declare("music", "theme", "music/theme.ogg")                  // any kind, by name
Assets.preloadSmoothTextures("rlss/assets/textures", "global")       // alias = file name without extension
Assets.preloadSounds("rlss/assets/sounds", "global")
```

| Method                                                  | Effect                                                       |
|---------------------------------------------------------|--------------------------------------------------------------|
| `texture`, `smoothTexture`, `font`, `sound`, `music` (`alias, path[, group]`) | Declares; loads only if the group is loaded. Returns `false` for an invalid path or name. |
| `declare(kind, alias, path[, group])`, `undeclare(kind, alias)` | Same for any kind named by its `NAME`; removes an alias. |
| `preload(kind, directory, group)`, `preloadTextures`, `preloadSmoothTextures`, `preloadFonts`, `preloadSounds`, `preloadMusics` (`directory, group`) | Declares every matching file of a directory, in name order; they load with the group. Returns the count. |
| `loadGroup(group)` / `unloadGroup(group)` / `forgetGroup(group)` / `isGroupLoaded(group)` | Loads, releases, forgets (releases and removes the declarations), queries a group. |
| `loadGroupStep(group, maxLoads)` / `groupProgress(group)` | Loads a few files per frame behind a loading screen; `true` once done / ratio from 0 to 1. |
| `textureId`, `fontId`, `soundId`, `musicId` (`alias`), `id(kind, alias)` | Handle of a loaded asset, `0` otherwise. |
| `status(kind, alias)`                                   | `"unknown"`, `"declared"`, `"loaded"` or `"failed"`.         |
| `isLoaded(alias)`, `hasKind(kind)`                      | Whether an asset of any kind is loaded under that alias; whether a kind exists. |

An **alias** never holds `/`, `\` or `.`: anything that does is a path. So a typo in an alias is reported as an
unknown alias, never looked for on the disk.

**Hot reload.** Declaring the same asset again changes nothing, so the modules can be hot reloaded: they run again and
nothing is loaded twice. The declarations made by scripts while they run (module top level) are replaced by the new
run: a line deleted from the script releases its asset at the next reload. An alias declared again for another file
names the new file in all its groups (with a warning), the old file is released if nothing else needs it, and the new
file takes the old one's handle, so the entities that kept it draw the new file. Declarations made from C++, or from
script callbacks such as `onEnter`, are kept, even when a script also made them. A modified module is first validated
in a staging VM, where `core/Assets` changes nothing; if the reload is rejected, or a module fails while it runs again,
no declaration is dropped.

**Untrusted input.** A value of the wrong type (or an unknown kind) aborts the calling fiber with an error. An empty,
too long (`MAX_PATH_LENGTH`, `MAX_NAME_LENGTH`) or NUL-holding path or name is rejected with a warning, and so is a
path leaving the working directory (absolute, drive letter, or starting with `..`). Scripts cannot unload or forget the
`"global"` group, which C++ code relies on for the whole run. A directory preload takes at most `MAX_DIRECTORY_FILES`
files (`"."` names the working directory) and skips a file whose alias already names another file (`ship.jpg` next to
`ship.png`, or an alias another group declared). The `smooth` option only applies to textures. Symbolic links are
followed: the declaration scripts are trusted game data, not downloaded content. Warnings cut long names, so a script cannot flood the logs.

## Content root

Relative paths are resolved against the **content root**, given to `AssetPlugin` (and to `ScriptPlugin` for the
script files), so a game runs from any working directory. The games pass `engine::contentRoot()`
(`core/ContentRoot.hpp`):

1. the `ENGINE_CONTENT_ROOT` environment variable, if it names a directory (content installed apart from the binary);
2. otherwise the directory of the executable, which the build puts at the repository root;
3. otherwise (platform cannot tell) an empty root: the working directory, as before.

Aliases and keys keep the path as declared (`"sprites/ship.png"`); only the loader receives the resolved path. An
absolute path is used as is. Tests and tools that pass no root keep reading from the working directory.

## Using assets from C++

```cpp
auto& manager = registry.getResource<assets::AssetManager>();
manager.loadGroup(assets::AssetManager::GLOBAL_GROUP);
const ecs::TextureId ship = manager.get<assets::Texture>("player_ship");
if (manager.status<assets::Texture>("player_ship") == assets::AssetStatus::Failed) { /* missing file */ }

// A game without Wren declares in C++ instead:
manager.declare<assets::Sound>("jump", "sounds/jump.wav", "level1");
manager.load<assets::Texture>("sprites/boss.png", "level1");   // a path works as well as an alias
```

`load` declares the asset in the group too, so it comes back when the group is loaded again.

A client prefab (`ClientEnemy`, `ClientBullet`) takes its texture by alias: `PrefabParams{{"texture", "bydos"}}`. An
alias that is not loaded is reported once and drawn as the checkerboard.

## Groups and scenes

With `assets::SceneAssetsPlugin`, a scene's group is the group **named like the scene**: it is loaded on
`scene::SceneEntering`, before the scene's `onEnter` (so the scene finds its handles), and unloaded on
`scene::SceneExited`, or on `scene::SceneEnterAborted` when `onEnter` throws. A scene called `"global"` changes
nothing, and a scene whose group declares nothing costs nothing.

```wren
Assets.texture("boss", "sprites/boss.png", "Level1")   // loaded while the "Level1" scene is on the stack
```

A scene can also manage groups by hand, in C++ or in a scripted scene (`Assets.loadGroup("level1")` in `onEnter`,
`Assets.unloadGroup("level1")` in `onExit`).

An asset in both `"global"` and `"Level1"` stays loaded when the level exits. Declarations are kept after an unload,
so entering the level again reloads the same files with the same handles, and going back and forth between scenes
keeps the number of loaded textures (`loadedCount<Texture>()`) flat. Both sample games keep their assets in
`"global"` today: R-Type resolves its sprite handles once at start-up (and its starfield runs behind every menu), and
RLSS draws its arena behind its menus. A level with assets of its own declares them in the level's group.

### Loading screens

`loadGroup` loads a whole group in the current frame. For a big group, call `loadGroupStep(group, n)` once per frame
and draw `groupProgress(group).ratio()` until `done()`: the first step marks the group loaded and makes it hold all its
assets, each step loads at most `n` files. Loading is synchronous within a step (decoding and GPU upload happen on the
main thread).

## Failures

| Case                          | Result                                                                      |
|-------------------------------|-----------------------------------------------------------------------------|
| Missing texture file          | `Failed`; `get` serves the magenta and black checkerboard (`AssetLoader::fallback`, `TextureStore::placeholder()`); `loadGroup` returns `false`; a warning. |
| Missing font                  | `Failed`; `get` returns `NO_FONT`: the backend's default font.             |
| Missing sound or music        | `Failed`; `NO_SOUND` / `NO_MUSIC`: silent. Not retried on every request.  |
| No loader (headless, no audio)| `Declared`: requests are recorded and served if a loader is installed later.|
| Unknown alias                 | `Unknown`; `get` returns `Kind::NONE`; `load` warns once.                   |

The R-Type client stops at start-up when one of its sprites is `Failed` (run it from the repository root); RLSS logs
it and draws the checkerboard. Warnings are printed with an `[assets]` prefix and the latest `MAX_KEPT_WARNINGS` are
kept in `AssetManager::warnings()`.

## Threads

The manager is not thread-safe: use it from the thread that created it (the main thread). Debug builds assert it on
every call.

## Tests

- `test_assets` (`tests/assets/AssetManagerTests.cpp`): a hundred loads of a file call the loader once; aliases and
  path spellings share one load; groups release only what no other group holds; stable handles across group reloads
  and loader restarts; failed assets and the fallback; alias rebinding releases the old file; undeclare and
  `forgetGroup`; options; a game's own kind; step-by-step loading; directory preloads; hot reload of script
  declarations; `StableSlots`.
- `test_assets_scripting` (`tests/assets/AssetScriptingTests.cpp`): the declaration script, `core/Assets` from Wren,
  wrong argument types, confined paths, the protected `"global"` group, step-by-step loading, kinds by name, hot
  reload (nothing loaded twice, a deleted line releases its asset).
- `test_assets_scene` (`tests/assets/SceneAssetsTests.cpp`): a scene's group is loaded before `onEnter` and released
  on exit or on a failed enter.
- `test_assets_audio` (`tests/assets/AudioAssetsTests.cpp`): the audio loaders, stable sound handles, a muted backend.
- `test_render`: `TextureStore::unload` and the placeholder; `test_client_collisions`: the prefab `texture` alias.
