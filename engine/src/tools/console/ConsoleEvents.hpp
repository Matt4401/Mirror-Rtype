/**
 * @file ConsoleEvents.hpp
 * @brief Events published by the ConsolePlugin.
 */

#pragma once

#include <string>

namespace tools {

/**
 * @brief A CVar changed value (from the console, from code, or because cheats got disabled).
 *
 * Published once per changed variable, at the next `ConsolePlugin::preUpdate`, so the simulation steps of that frame
 * see it. Read the new value from the `CVarManager` service. Code that reads a `CVarRef` every frame does not need it.
 */
struct CVarChanged {
    /** @brief Name of the variable. */
    std::string name;
};

}  // namespace tools
