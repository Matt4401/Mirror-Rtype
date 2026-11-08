/**
 * @file WrenConsole.hpp
 * @brief Runs Wren code typed in the console or read by `exec`, and reports its output and errors to the console.
 */

#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace scripting {
class WrenVM;
}

namespace tools {

class CommandContext;

/**
 * @brief Runs Wren code for the console, on the script VM of the program.
 *
 * - `evaluate` runs a line typed after `wren`, in the `console` module: its top-level variables stay defined from one
 *   line to the next (`wren var speed = 3`, then `wren speed * 2`). An expression prints its value (`=> 6`).
 * - `runFile` runs a `.wren` file read by `exec`, in a module of its own.
 * - While the code runs, `System.print` and the errors go to the console output instead of the terminal; the previous
 *   VM handlers are restored afterwards.
 *
 * The engine API is reachable through the core modules: `import "core/Engine" for Engine`.
 */
class WrenConsole {
  public:
    /** @brief Module the lines typed in the console run in. */
    static constexpr std::string_view MODULE = "console";

    /**
     * @brief Creates the console runner.
     * @param vm Script VM; must outlive this object.
     */
    explicit WrenConsole(scripting::WrenVM& vm) noexcept : _vm(&vm) {}

    /**
     * @brief Runs a line of Wren code; an expression prints its value.
     * @param code Code typed after `wren`.
     * @param context Context of the command: output and errors go through it.
     * @return `true` if the code ran without error.
     */
    bool evaluate(std::string_view code, CommandContext& context);

    /**
     * @brief Runs the content of a `.wren` file in a module of its own.
     * @param source File content.
     * @param path Path as typed, used in the module name shown by errors.
     * @param context Context of the command: output and errors go through it.
     * @return `true` if the code ran without error.
     */
    bool runFile(std::string_view source, std::string_view path, CommandContext& context);

  private:
    /** @brief What a run of the VM produced. */
    struct Outcome {
        /** @brief Whether the code compiled and ran without error. */
        bool ok = false;
        /** @brief Whether the code did not compile (it did not run at all then). */
        bool compileError = false;
        /** @brief Text written by `System.print`. */
        std::string output;
        /** @brief Error message and stack trace. */
        std::string error;
    };

    /**
     * @brief Interprets code with the console output handlers installed.
     * @param module Module the code runs in.
     * @param source Code.
     * @return What the run produced.
     */
    Outcome interpret(std::string_view module, std::string_view source);

    /**
     * @brief Writes the output and the error of a run to the console.
     * @param outcome Run.
     * @param context Context of the command.
     * @return `outcome.ok`.
     */
    static bool report(const Outcome& outcome, CommandContext& context);

    /** @brief Script VM, never null. */
    scripting::WrenVM* _vm;
    /** @brief Files run so far, numbering their modules (a module cannot be defined twice). */
    std::size_t _files = 0;
};

}  // namespace tools
