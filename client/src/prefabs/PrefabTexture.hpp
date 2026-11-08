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
#include "components/Renderable.hpp"
#include "ecs/Registry.hpp"
#include "prefabs/PrefabParams.hpp"

namespace client::prefabs {

/**
 * @brief Gets the texture named by the `texture` parameter (an asset alias, such as `"bydos"`), or else the raw
 * `textureId` parameter.
 *
 * @param registry Registry holding the `assets::AssetManager`, if any.
 * @param params Spawn parameters.
 * @return The texture, or `ecs::NO_TEXTURE`.
 */
[[nodiscard]] inline ecs::TextureId prefabTexture(const ecs::Registry& registry,
                                                  const ::prefabs::PrefabParams& params) {
    if (const std::optional<std::string> alias = params.find<std::string>("texture");
        alias && registry.hasResource<assets::AssetManager>()) {
        return registry.getResource<assets::AssetManager>().get<assets::Texture>(*alias);
    }
    return static_cast<ecs::TextureId>(params.get<std::int64_t>("textureId", 0));
}

}  // namespace client::prefabs
