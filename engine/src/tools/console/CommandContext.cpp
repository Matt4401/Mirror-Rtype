#include "tools/console/CommandContext.hpp"

#include <string_view>

#include "tools/console/Console.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace tools {

CVarManager& CommandContext::cvars() const noexcept { return _console->cvars(); }

void CommandContext::print(std::string_view text) const { _console->print(OutputLevel::Info, text); }

void CommandContext::error(std::string_view text) {
    _failed = true;
    _console->print(OutputLevel::Error, text);
}

}  // namespace tools
