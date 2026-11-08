#include "tools/scripting/WrenConsole.hpp"

#include <string>
#include <string_view>
#include <utility>

#include "scripting/WrenVM.hpp"
#include "scripting/detail/WrenApi.hpp"
#include "tools/console/CommandContext.hpp"

namespace tools {

WrenConsole::Outcome WrenConsole::interpret(std::string_view module, std::string_view source) {
    Outcome outcome;

    // Puts back the handlers of the VM when the run ends, even if a handler throws.
    class HandlerScope {
      public:
        HandlerScope(scripting::WrenVM& vm, Outcome& outcome)
            : _vm(&vm), _write(vm.writeHandler()), _error(vm.errorHandler()) {
            vm.setWriteHandler([&outcome](std::string_view text) { outcome.output += text; });
            vm.setErrorHandler([&outcome](WrenErrorType type, std::string_view /*module*/, int /*line*/,
                                          std::string_view /*message*/) {
                outcome.compileError = outcome.compileError || type == WREN_ERROR_COMPILE;
            });
        }
        ~HandlerScope() {
            _vm->setWriteHandler(std::move(_write));
            _vm->setErrorHandler(std::move(_error));
        }
        HandlerScope(const HandlerScope&) = delete;
        HandlerScope& operator=(const HandlerScope&) = delete;
        HandlerScope(HandlerScope&&) = delete;
        HandlerScope& operator=(HandlerScope&&) = delete;

      private:
        scripting::WrenVM* _vm;
        scripting::WrenVM::WriteHandler _write;
        scripting::WrenVM::ErrorHandler _error;
    };

    const HandlerScope scope(*_vm, outcome);
    if (auto result = _vm->interpret(module, source); !result) {
        outcome.error = std::move(result.error());
        return outcome;
    }
    outcome.ok = true;
    return outcome;
}

bool WrenConsole::report(const Outcome& outcome, CommandContext& context) {
    const auto trimmed = [](std::string_view text) {
        while (text.ends_with('\n')) {
            text.remove_suffix(1);
        }
        return text;
    };
    if (const std::string_view output = trimmed(outcome.output); !output.empty()) {
        context.print(output);
    }
    if (!outcome.ok) {
        context.error(trimmed(outcome.error));
    }
    return outcome.ok;
}

bool WrenConsole::evaluate(std::string_view code, CommandContext& context) {
    // First as an expression whose value is printed. The block keeps its variable local; the code sits alone on its
    // line, so a trailing `// comment` only comments itself out.
    const std::string expression =
        "{\n  var result_ = " + std::string(code) + "\n  if (result_ != null) System.print(\"=> %(result_)\")\n}\n";
    Outcome outcome = interpret(MODULE, expression);
    // Not an expression (a `var`, a `class`, several statements...): run it as it is. Only a compile error retries:
    // code that started running is never run twice.
    if (!outcome.ok && outcome.compileError) {
        outcome = interpret(MODULE, code);
    }
    return report(outcome, context);
}

bool WrenConsole::runFile(std::string_view source, std::string_view path, CommandContext& context) {
    const std::string module = "exec:" + std::string(path) + "#" + std::to_string(++_files);
    return report(interpret(module, source), context);
}

}  // namespace tools
