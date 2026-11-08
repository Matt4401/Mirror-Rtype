/**
 * @file AssetContext.hpp
 * @brief State the asset manager shares with the cache of every kind: warnings, loaded groups, script window.
 */

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "assets/detail/Diagnostics.hpp"

namespace assets::detail {

/** @brief Owned by the `AssetManager`, referenced by each of its caches. */
struct AssetContext {
    /** @brief Warning sink. */
    Diagnostics diagnostics;
    /** @brief Groups loaded with `loadGroup` (or being loaded step by step) and not unloaded since. */
    std::vector<std::string> loadedGroups;
    /** @brief True while a declaration script runs: the declarations made then can be replaced by a hot reload. */
    bool scriptWindow = false;

    /**
     * @brief Tells whether a group is loaded.
     * @param group Group name.
     * @return True between `loadGroup` and `unloadGroup`.
     */
    [[nodiscard]] bool isGroupLoaded(const std::string_view group) const {
        return std::ranges::find(loadedGroups, group) != loadedGroups.end();
    }
};

}  // namespace assets::detail
