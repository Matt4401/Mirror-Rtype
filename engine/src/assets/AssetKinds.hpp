/**
 * @file AssetKinds.hpp
 * @brief Kinds of assets the asset manager handles, their handle types and the loaders the backends plug in.
 *
 * The set of kinds is open: any type satisfying `AssetKind` can be stored in an `AssetManager`, so a game or a module
 * adds its own kinds (levels, shaders, atlases...) without touching the engine. The four kinds below are the ones the
 * engine's render and audio modules load; their handle types come from `engine_core`.
 */

#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "components/Renderable.hpp"
#include "core/AudioHandles.hpp"
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

/**
 * @brief How an asset is loaded; options a kind does not use are ignored.
 *
 * The same file loaded with different options is two assets (a nearest and a smooth texture of one image), so the
 * result never depends on which declaration came first.
 */
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
    /** @brief Name used in logs and by scripts (`Assets.declare("texture", ...)`). */
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
    /** @brief Name used in logs and by scripts. */
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
    /** @brief Name used in logs and by scripts. */
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
    /** @brief Name used in logs and by scripts. */
    static constexpr std::string_view NAME = "music";
    /** @brief File extensions picked by `AssetManager::preloadDirectory`. */
    static constexpr std::array<std::string_view, 6> EXTENSIONS = {
        ".ogg", ".mp3", ".wav", ".flac", ".xm", ".mod",
    };
};

/**
 * @brief A kind of asset: an unsigned integer handle type, its "none" value, a unique name and file extensions.
 *
 * Handles are integers so that scripts can hold them and the manager can store every kind behind one interface.
 * @tparam Kind Kind to check.
 */
template <typename Kind>
concept AssetKind = requires {
    typename Kind::Id;
    requires std::unsigned_integral<typename Kind::Id>;
    { Kind::NONE } -> std::convertible_to<typename Kind::Id>;
    { Kind::NAME } -> std::convertible_to<std::string_view>;
    { Kind::EXTENSIONS.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief What a backend plugs into the manager to load and release one kind of asset.
 *
 * Only `load` and `unload` are required.
 *
 * Handles are kept stable: when an asset is loaded again (its group was unloaded then loaded, or the backend was
 * restarted), `load` receives the handle it had before as `preferred` and should return it when that handle is free,
 * so components and C++ code that kept it see the asset again. A backend that cannot honour it returns a new handle.
 *
 * @tparam Kind Kind of asset.
 */
template <AssetKind Kind>
struct AssetLoader {
    /**
     * @brief Loads a file: its normalised path (`/` separators), resolved against the manager's root; returns
     * `Kind::NONE` when it cannot be loaded.
     */
    ecs::MoveOnlyFunction<typename Kind::Id(const std::string& path, const LoadOptions& options,
                                            typename Kind::Id preferred)>
        load;
    /** @brief Releases a handle returned by `load`. */
    ecs::MoveOnlyFunction<void(typename Kind::Id id)> unload;
    /**
     * @brief Optional: handle served instead of an asset that failed to load (the missing texture checkerboard).
     * Owned by the backend: the manager never passes it to `unload`.
     */
    ecs::MoveOnlyFunction<typename Kind::Id()> fallback;
    /**
     * @brief Optional: keeps every handle up to `last` out of the backend's fresh allocations, so the handles the
     * manager will ask for again as `preferred` are still free when it does. Called when the loader is installed.
     */
    ecs::MoveOnlyFunction<void(typename Kind::Id last)> reserve;
};

/** @brief Where an asset stands in the manager. */
enum class AssetStatus : std::uint8_t {
    /** @brief No such alias or path. */
    Unknown,
    /** @brief Known, but not loaded: its groups are not loaded, or no loader is installed yet. */
    Declared,
    /** @brief Loaded: `get` returns its handle. */
    Loaded,
    /** @brief The loader could not load it: `get` returns the fallback handle, if the backend has one. */
    Failed,
};

/** @brief How far the loading of a group went. */
struct GroupProgress {
    /** @brief Assets declared in the group, every kind together. */
    std::size_t total = 0;
    /** @brief Assets of the group that are loaded. */
    std::size_t loaded = 0;
    /** @brief Assets of the group the loader could not load. */
    std::size_t failed = 0;
    /** @brief Assets of the group whose kind has no loader yet: loaded once one is installed. */
    std::size_t waiting = 0;

    /**
     * @brief Tells whether nothing more can be loaded now.
     * @return True when every asset is loaded, failed or waiting for a loader.
     */
    [[nodiscard]] constexpr bool done() const noexcept { return loaded + failed + waiting >= total; }

    /**
     * @brief Gets the part of the group that is settled, for a loading bar.
     * @return 1 for an empty group, otherwise (loaded + failed) / (total - waiting).
     */
    [[nodiscard]] constexpr float ratio() const noexcept {
        const std::size_t reachable = total - waiting;
        return reachable == 0 ? 1.0F : static_cast<float>(loaded + failed) / static_cast<float>(reachable);
    }
};

}  // namespace assets
