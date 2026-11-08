/**
 * @file AssetScriptModule.hpp
 * @brief Wren source of the `core/Assets` built-in module.
 */

#pragma once

#include <cstdint>
#include <string_view>

namespace assets {

/** @brief Import name of the assets module. */
inline constexpr std::string_view ASSETS_MODULE_NAME = "core/Assets";

/** @brief Kind codes passed by `core/Assets` to its foreign methods. */
enum class ScriptAssetKind : std::uint8_t {
    /** @brief Texture sampled with the nearest pixel. */
    Texture = 0,
    /** @brief Texture with smooth (mipmapped) filtering. */
    SmoothTexture = 1,
    /** @brief Font. */
    Font = 2,
    /** @brief Sound effect. */
    Sound = 3,
    /** @brief Music stream. */
    Music = 4,
};

/**
 * @brief Wren source of `core/Assets`.
 *
 * Declarations (`texture`, `sound`...) only record the assets of a group; `loadGroup` loads them, `unloadGroup`
 * releases them. Without a group, `"global"` is used. The `*Id` getters return 0 for an unknown or unloaded asset.
 */
inline constexpr std::string_view ASSETS_MODULE_SOURCE = R"wren(
class Assets {
    foreign static declare_(kind, alias, path, group)
    foreign static preload_(kind, directory, group)
    foreign static id_(kind, alias)
    foreign static loadGroup(group)
    foreign static unloadGroup(group)
    foreign static isGroupLoaded(group)

    static texture(alias, path) { declare_(0, alias, path, "global") }
    static texture(alias, path, group) { declare_(0, alias, path, group) }
    static smoothTexture(alias, path) { declare_(1, alias, path, "global") }
    static smoothTexture(alias, path, group) { declare_(1, alias, path, group) }
    static font(alias, path) { declare_(2, alias, path, "global") }
    static font(alias, path, group) { declare_(2, alias, path, group) }
    static sound(alias, path) { declare_(3, alias, path, "global") }
    static sound(alias, path, group) { declare_(3, alias, path, group) }
    static music(alias, path) { declare_(4, alias, path, "global") }
    static music(alias, path, group) { declare_(4, alias, path, group) }

    static preloadTextures(directory, group) { preload_(0, directory, group) }
    static preloadSmoothTextures(directory, group) { preload_(1, directory, group) }
    static preloadSounds(directory, group) { preload_(3, directory, group) }

    static textureId(alias) { id_(0, alias) }
    static fontId(alias) { id_(2, alias) }
    static soundId(alias) { id_(3, alias) }
    static musicId(alias) { id_(4, alias) }
    static isLoaded(alias) {
        return textureId(alias) != 0 || fontId(alias) != 0 || soundId(alias) != 0 || musicId(alias) != 0
    }
}
)wren";

}  // namespace assets
