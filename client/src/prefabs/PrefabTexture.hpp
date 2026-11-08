/**
 * @file PrefabTexture.hpp
 * @brief Resolves the texture of a client prefab from its spawn parameters.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPath.hpp"
#include "components/Renderable.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

/**
 * @brief Gets the texture named by the `texture` parameter (an asset alias, such as `"bydos"`), or else the raw
 * `textureId` parameter.
 *
 * An alias that is not loaded (unknown, or its group is not loaded) is reported once and drawn as the fallback
 * checkerboard, so a typo in a prefab never makes an entity silently invisible.
 *
 * @param registry Registry holding the `assets::AssetManager`, if any.
 * @param params Spawn parameters.
 * @return The texture, or `ecs::NO_TEXTURE`.
 */
[[nodiscard]] inline ecs::TextureId prefabTexture(ecs::Registry& registry, const ::prefabs::PrefabParams& params) {
    const std::optional<std::string> alias = params.find<std::string>("texture");
    if (!alias || !registry.hasResource<assets::AssetManager>()) {
        return static_cast<ecs::TextureId>(params.get<std::int64_t>("textureId", 0));
    }
    auto& manager = registry.getResource<assets::AssetManager>();
    const ecs::TextureId id = manager.get<assets::Texture>(*alias);
    if (id != ecs::NO_TEXTURE) {
        return id;
    }
    manager.warnOnce("prefab-texture:" + *alias,
                     "prefab texture " + assets::quoteForLog(*alias) +
                         " is not loaded: drawn as the fallback (is it declared, and its group loaded?)");
    return manager.fallback<assets::Texture>();
}

}  // namespace client::prefabs
