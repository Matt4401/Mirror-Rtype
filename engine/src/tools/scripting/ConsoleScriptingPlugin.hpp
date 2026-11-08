/**
 * @file ConsoleScriptingPlugin.hpp
 * @brief Bridge between the console and Wren: the `wren` command and `.wren` files for `exec`.
 */

#pragma once

#include <memory>
#include <string_view>

#include "core/IPlugin.hpp"
#include "core/PluginBuilder.hpp"
#include "tools/scripting/WrenConsole.hpp"

namespace tools {

/**
 * @brief Lets the console run Wren code, when the program has both the console and the script plugins.
 *
 * - `wren <code>`: runs a line of Wren in the `console` module; an expression prints its value. The command is raw, so
 *   quotes, `;` and `//` reach Wren untouched.
 * - `exec <file>.wren`: runs a Wren file of the script directory.
 *
 * Running code can change anything in the game, so both are cheats: refused while cheats are disallowed (competitive
 * servers). Neither `engine_tools` nor `engine_scripting` knows about the other; this module joins them.
 */
class ConsoleScriptingPlugin final : public engine::IPlugin {
  public:
    /** @brief Name of the command. */
    static constexpr std::string_view COMMAND = "wren";
    /** @brief Extension of the files run by `exec`. */
    static constexpr std::string_view EXTENSION = ".wren";

    /**
     * @brief Gets plugin name.
     * @return String view "ConsoleScriptingPlugin".
     */
    [[nodiscard]] std::string_view name() const noexcept override { return "ConsoleScriptingPlugin"; }

    /**
     * @brief Declares the console and the script plugins.
     * @param builder Dependency builder.
     */
    void build(engine::PluginBuilder& builder) override;

    /**
     * @brief Adds the `wren` command and the `.wren` script runner to the console.
     * @param engine Host engine.
     */
    void init(engine::Engine& engine) override;

    /**
     * @brief Removes the command and the runner.
     * @param engine Host engine.
     */
    void shutdown(engine::Engine& engine) override;

  private:
    /** @brief Runs the code, created at `init` on the script VM. */
    std::unique_ptr<WrenConsole> _wren;
};

}  // namespace tools
