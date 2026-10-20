/**
 * @file PrefabRegistry.hpp
 * @brief Registers every R-Type prefab into a `PrefabManager`.
 */

#pragma once

#include "prefabs/PrefabManager.hpp"

namespace prefabs {

/**
 * @brief Registers all the R-Type prefabs and their variants under their default names.
 * @param manager Prefab manager receiving the factories.
 */
void registerRtypePrefabs(PrefabManager& manager);

}  // namespace prefabs
