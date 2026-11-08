# Assets

The `engine_assets` module is the engine's asset manager. Games name their textures, fonts, sounds and musics by
**alias** (`"player_ship"`) instead of by file, and gather them into **groups**: `"global"` for what the whole game
uses, one group per scene or level for the rest. A file is loaded **once**, however many times it is declared or
requested, and is released when the last group holding it is unloaded.

The module is generic and headless. It depends only on `engine_core` and never on raylib: the rendering and audio
modules plug their loaders into it, so the server can link it and the tests run without a window.

## Why a manager

Assets were never entities: an entity only carries a handle (`Renderable::textureId`, `AudioEmitter::sound`), and the
renderer resolves it in O(1). The manager changes nothing per frame. It fixes what happens around loading:

| Before                                                      | With `engine_assets`                                         |
|-------------------------------------------------------------|---------------------------------------------------------------|
| Hard-coded paths in each game plugin                        | Aliases declared in one Wren script per game                  |
| Prefabs took a raw `textureId` number                       | Prefabs take a `texture` alias                                 |
| Loading a file twice took twice the memory                  | One load per normalised path (`a/../a/x.png` is `a/x.png`)     |
| `TextureStore` and `FontStore` could not release one asset  | Groups release what no other group holds, so scene changes do not leak VRAM |
| Three APIs and three ways to fail (exception, log, silence) | One API; a missing texture is drawn as a magenta checkerboard |

## Concepts

| Type                   | Header                                     | Role                                                        |
|------------------------|--------------------------------------------|-------------------------------------------------------------|
| `AssetManager`         | `assets/AssetManager.hpp`                  | Registry resource: declarations, groups, handles.           |
| `Texture`, `Font`, `Sound`, `Music` | `assets/AssetKinds.hpp`       | Kinds, with their handle type (`Kind::Id`, `Kind::NONE`).   |
| `AssetLoader<Kind>`    | `assets/AssetKinds.hpp`                    | `load` / `unload` callbacks a backend plugs in.             |
| `AssetPlugin`          | `assets/AssetPlugin.hpp`                   | Adds the manager to the registry, releases it at shutdown.  |
| `AssetScriptingPlugin` | `assets/scripting/AssetScriptingPlugin.hpp`| Wren module `core/Assets` and the game's declaration script.|

Handles are the engine's existing types (`ecs::TextureId`, `engine::FontId`, `audio::SoundId`, `audio::MusicId`):
components, the render system and the audio system are unchanged.

## Lifecycle

```
AssetPlugin::init          the AssetManager resource is added
AssetScriptingPlugin::init the declaration script runs: aliases and groups are recorded
RenderPlugin / AudioPlugin plug in their loaders: what a group already asked for is loaded now
game plugins / scenes      loadGroup("global"), get<Texture>("player_ship"), unloadGroup("level1")...
RenderPlugin / AudioPlugin shutdown: detachLoader releases their assets while the window and the device still exist
AssetPlugin::shutdown      unloadAll, then the resource is removed
```

`RenderPlugin` and `AudioPlugin` declare `AssetPlugin` as an **optional** dependency: with it they start after it and
plug in their loaders, without it they behave as before.

## Declaring assets in Wren

Each game has one declaration module, given to `AssetScriptingPlugin` and run from the repository root:

```wren
import "core/Assets" for Assets

Assets.texture("player_ship", "sprites/r-typesheet42.gif")          // group "global"
Assets.smoothTexture("stadium", "rlss/assets/textures/stadium.png")   // mipmapped, trilinear
Assets.sound("boss_roar", "sounds/boss.wav", "level3")
Assets.preloadSmoothTextures("rlss/assets/textures", "global")       // alias = file name without extension
Assets.preloadSounds("rlss/assets/sounds", "global")
```

| Method                                                  | Effect                                                       |
|---------------------------------------------------------|--------------------------------------------------------------|
| `texture`, `smoothTexture`, `font`, `sound`, `music` (`alias, path[, group]`) | Declares; loads only if the group is loaded. Returns `false` for an invalid path or name. |
| `preloadTextures`, `preloadSmoothTextures`, `preloadSounds` (`directory, group`) | Declares and loads every matching file of a directory, in name order. Returns the count. |
| `loadGroup(group)` / `unloadGroup(group)` / `isGroupLoaded(group)` | Loads, releases, queries a group.                  |
| `textureId`, `fontId`, `soundId`, `musicId` (`alias`)   | Handle of a loaded asset, `0` otherwise.                      |
| `isLoaded(alias)`                                       | Whether an asset of any kind is loaded under that alias.     |

Declaring the same asset again changes nothing, so the module can be **hot reloaded**: it runs again and nothing is
loaded twice. An alias declared again for another file names the new file (with a warning).

Script arguments are untrusted: a value of the wrong type aborts the calling fiber with an error, an empty, too long
(`MAX_PATH_LENGTH`, `MAX_NAME_LENGTH`) or NUL-holding path or name is rejected with a warning. A directory preload takes
at most `MAX_DIRECTORY_FILES` files.

## Using assets from C++

```cpp
auto& manager = registry.getResource<assets::AssetManager>();
manager.loadGroup(assets::AssetManager::GLOBAL_GROUP);
const ecs::TextureId ship = manager.get<assets::Texture>("player_ship");

// A game without Wren declares in C++ instead:
manager.declare<assets::Sound>("jump", "sounds/jump.wav", "level1");
manager.load<assets::Texture>("sprites/boss.png", "level1");   // a path works as well as an alias
```

A client prefab (`ClientEnemy`, `ClientBullet`) takes its texture by alias: `PrefabParams{{"texture", "bydos"}}`.

## Groups and scenes

A scene loads its group on entering and releases it on exiting, in C++ or in a scripted scene:

```wren
class Level1 {
    construct new() {}
    onEnter() { Assets.loadGroup("level1") }
    onExit() { Assets.unloadGroup("level1") }
}
```

An asset in both `"global"` and `"level1"` stays loaded when the level exits. Declarations are kept after an unload,
so entering the level again reloads the same files, and going back and forth between scenes keeps the number of
loaded textures (`loadedCount<Texture>()`) flat.

## Failures

| Case                          | Result                                                                      |
|-------------------------------|-----------------------------------------------------------------------------|
| Missing texture file          | The magenta and black checkerboard (`TextureStore::placeholder()`), and a log line. |
| Missing font                  | `NO_FONT`: the backend's default font.                                      |
| Missing sound or music        | `NO_SOUND` / `NO_MUSIC`: silent. Not retried on every request.              |
| No loader (headless, no audio)| Requests are recorded and served if a loader is installed later.           |
| Unknown alias                 | `get` returns `Kind::NONE`.                                                 |

Warnings are printed with an `[assets]` prefix and the latest `MAX_KEPT_WARNINGS` are kept in
`AssetManager::warnings()`.

## Tests

- `test_assets` (`tests/assets/AssetManagerTests.cpp`): a hundred loads of a file call the loader once; aliases and
  path spellings share one load; groups release only what no other group holds; scene round trips keep the count
  flat; detaching a loader releases its assets; invalid input is rejected; directory preloads.
- `test_assets_scripting` (`tests/assets/AssetScriptingTests.cpp`): the declaration script, `core/Assets` from Wren,
  wrong argument types, hot reload.
- `test_audio_plugin` and `test_render`: the audio loaders release their sounds at shutdown; `TextureStore::unload`.
