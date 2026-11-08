// Overlay shown over the game at the start and at the end of each level.
//
// A scene written in Wren (registered by client::ClientScenesPlugin): the banner is a C++ prefab, owned by this
// scene because the scene spawns it, so it disappears when the scene leaves the stack.
import "core/Engine" for Engine
import "core/Scene" for Scene

class LevelTransition {
    static DURATION { 2.5 }

    construct new() {}

    onEnter() {
        Engine.spawnPrefab("level_banner", 0, 0)
        Engine.wait(LevelTransition.DURATION)
        // Only close the banner: if another scene was pushed over it meanwhile, it stays until that one is gone.
        if (Scene.current == "LevelTransition") {
            Scene.pop()
        }
    }
}
