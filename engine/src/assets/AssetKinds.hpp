/**
 * @file AssetKinds.hpp
 * @brief Kinds of assets the asset manager handles, their handle types and the loaders the backends plug in.
 */

#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <string>
#include <string_view>

#include "audio/types/AudioConstants.hpp"
#include "audio/types/AudioTypes.hpp"
#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "utils/MoveOnlyFunction.hpp"

namespace assets {

/** @brief How a texture is sampled when drawn bigger or smaller than its pixel size. */
enum class TextureFilter : std::uint8_t {
    /** @brief Nearest pixel: crisp pixel art. */
    Nearest,
    /** @brief Mipmapped trilinear filtering: smooth artwork that stays clean when zoomed in or out. */
    Smooth,
};

/** @brief How an asset is loaded; options a kind does not use are ignored. */
struct LoadOptions {
    /** @brief Sampling of a texture. */
    TextureFilter filter = TextureFilter::Nearest;

    /** @brief Equality, so that a declaration repeated with the same options is recognised. */
    bool operator==(const LoadOptions&) const = default;
};

/** @brief Image drawn by the rendering module, referenced by `ecs::Renderable::textureId`. */
struct Texture {
    /** @brief Handle type. */
    using Id = ecs::TextureId;
    /** @brief Handle of no texture. */
    static constexpr Id NONE = ecs::NO_TEXTURE;
    /** @brief Name used in logs. */
    static constexpr std::string_view NAME = "texture";
    /** @brief File extensions picked by `AssetManager::preloadDirectory`. */
    static constexpr std::array<std::string_view, 7> EXTENSIONS = {
        ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".tga", ".qoi",
    };
};

/** @brief Font drawn by the screen draw service. */
struct Font {
    /** @brief Handle type. */
    using Id = engine::FontId;
    /** @brief Handle of no font: the backend's default font is used. */
    static constexpr Id NONE = engine::NO_FONT;
    /** @brief Name used in logs. */
    static constexpr std::string_view NAME = "font";
    /** @brief File extensions picked by `AssetManager::preloadDirectory`. */
    static constexpr std::array<std::string_view, 2> EXTENSIONS = {
        ".ttf",
        ".otf",
    };
};

/** @brief Sound effect, fully loaded in memory, referenced by `ecs::AudioEmitter::sound` and `PlaySoundEvent`. */
struct Sound {
    /** @brief Handle type. */
    using Id = audio::SoundId;
    /** @brief Handle of no sound: playing it does nothing. */
    static constexpr Id NONE = audio::NO_SOUND;
    /** @brief Name used in logs. */
    static constexpr std::string_view NAME = "sound";
    /** @brief File extensions picked by `AssetManager::preloadDirectory`. */
    static constexpr std::array<std::string_view, 4> EXTENSIONS = {
        ".wav",
        ".ogg",
        ".mp3",
        ".flac",
    };
};

/** @brief Music stream, decoded while it plays, referenced by `PlayMusicEvent`. */
struct Music {
    /** @brief Handle type. */
    using Id = audio::MusicId;
    /** @brief Handle of no music: playing it does nothing. */
    static constexpr Id NONE = audio::NO_MUSIC;
    /** @brief Name used in logs. */
    static constexpr std::string_view NAME = "music";
    /** @brief File extensions picked by `AssetManager::preloadDirectory`. */
    static constexpr std::array<std::string_view, 6> EXTENSIONS = {
        ".ogg", ".mp3", ".wav", ".flac", ".xm", ".mod",
    };
};

/**
 * @brief One of the kinds above.
 * @tparam Kind Kind to check.
 */
template <typename Kind>
concept AssetKind = requires {
    typename Kind::Id;
    { Kind::NONE } -> std::convertible_to<typename Kind::Id>;
    { Kind::NAME } -> std::convertible_to<std::string_view>;
    Kind::EXTENSIONS.size();
};

/**
 * @brief What a backend plugs into the manager to load and release one kind of asset.
 *
 * `load` returns `Kind::NONE` when the file cannot be loaded; it may also return a placeholder handle instead (the
 * missing texture checkerboard), which `unload` must then ignore.
 *
 * @tparam Kind Kind of asset.
 */
template <AssetKind Kind>
struct AssetLoader {
    /** @brief Loads a file: its normalised path (`/` separators), resolved against the manager's root. */
    ecs::MoveOnlyFunction<typename Kind::Id(const std::string& path, const LoadOptions& options)> load;
    /** @brief Releases a handle returned by `load`. */
    ecs::MoveOnlyFunction<void(typename Kind::Id id)> unload;
};

}  // namespace assets
