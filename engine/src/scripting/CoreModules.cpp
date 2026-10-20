/**
 * @file CoreModules.cpp
 * @brief Implementation of built-in Wren module retrieval.
 */

#include "scripting/CoreModules.hpp"

#include <string_view>

namespace scripting {

std::string_view getCoreModuleSource(std::string_view moduleName) noexcept {
    if (moduleName == "core/Engine") {
        return CORE_ENGINE_SOURCE;
    }
    if (moduleName == "core/Entity") {
        return CORE_ENTITY_SOURCE;
    }
    if (moduleName == "core/World") {
        return CORE_WORLD_SOURCE;
    }
    if (moduleName == "core/Input") {
        return CORE_INPUT_SOURCE;
    }
    if (moduleName == "_internal") {
        return INTERNAL_DISPATCHER_SOURCE;
    }
    return {};
}

}  // namespace scripting
