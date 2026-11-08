/**
 * @file AssetScriptModule.hpp
 * @brief Wren source of the `core/Assets` built-in module.
 */

#pragma once

#include <string_view>

namespace assets {

/** @brief Import name of the assets module. */
inline constexpr std::string_view ASSETS_MODULE_NAME = "core/Assets";

/**
 * @brief Wren source of `core/Assets`.
 *
 * Kinds are named by their `NAME` (`"texture"`, `"font"`, `"sound"`, `"music"`, or a kind a game registered), so a
 * new kind needs no change here. Declarations only record the assets of a group; `loadGroup` loads them (or
 * `loadGroupStep`, a few per frame), `unloadGroup` releases them. Without a group, `"global"` is used. The id getters
 * return 0 for an unknown or unloaded asset.
 */
inline constexpr std::string_view ASSETS_MODULE_SOURCE = R"wren(
class Assets {
    foreign static declare_(kind, alias, path, group, smooth)
    foreign static preload_(kind, directory, group, smooth)
    foreign static undeclare(kind, alias)
    foreign static id(kind, alias)
    foreign static status(kind, alias)
    foreign static isLoaded(alias)
    foreign static hasKind(kind)
    foreign static loadGroup(group)
    foreign static loadGroupStep(group, maxLoads)
    foreign static groupProgress(group)
    foreign static unloadGroup(group)
    foreign static forgetGroup(group)
    foreign static isGroupLoaded(group)

    static declare(kind, alias, path) { declare_(kind, alias, path, "global", false) }
    static declare(kind, alias, path, group) { declare_(kind, alias, path, group, false) }
    static texture(alias, path) { declare_("texture", alias, path, "global", false) }
    static texture(alias, path, group) { declare_("texture", alias, path, group, false) }
    static smoothTexture(alias, path) { declare_("texture", alias, path, "global", true) }
    static smoothTexture(alias, path, group) { declare_("texture", alias, path, group, true) }
    static font(alias, path) { declare_("font", alias, path, "global", false) }
    static font(alias, path, group) { declare_("font", alias, path, group, false) }
    static sound(alias, path) { declare_("sound", alias, path, "global", false) }
    static sound(alias, path, group) { declare_("sound", alias, path, group, false) }
    static music(alias, path) { declare_("music", alias, path, "global", false) }
    static music(alias, path, group) { declare_("music", alias, path, group, false) }

    static preload(kind, directory, group) { preload_(kind, directory, group, false) }
    static preloadTextures(directory, group) { preload_("texture", directory, group, false) }
    static preloadSmoothTextures(directory, group) { preload_("texture", directory, group, true) }
    static preloadFonts(directory, group) { preload_("font", directory, group, false) }
    static preloadSounds(directory, group) { preload_("sound", directory, group, false) }
    static preloadMusics(directory, group) { preload_("music", directory, group, false) }

    static textureId(alias) { id("texture", alias) }
    static fontId(alias) { id("font", alias) }
    static soundId(alias) { id("sound", alias) }
    static musicId(alias) { id("music", alias) }
}
)wren";

}  // namespace assets
