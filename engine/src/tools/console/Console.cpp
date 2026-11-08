#include "tools/console/Console.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "tools/common/TextUtils.hpp"
#include "tools/console/CommandContext.hpp"
#include "tools/console/CommandLine.hpp"
#include "tools/console/ConsoleHelpers.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/console/ScriptFile.hpp"
#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarManager.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

void Console::addCommand(std::string name, Command command) {
    if (!CVarManager::isValidName(name)) {
        throw std::invalid_argument("Console: invalid command name '" + name + "'");
    }
    if (!command.handler) {
        throw std::invalid_argument("Console: command '" + name + "' has no handler");
    }
    _commands.insert_or_assign(std::move(name), std::move(command));
}

bool Console::removeCommand(std::string_view name) {
    const auto it = _commands.find(name);
    if (it == _commands.end()) {
        return false;
    }
    _commands.erase(it);
    return true;
}

const Command* Console::findCommand(std::string_view name) const noexcept {
    const auto it = _commands.find(name);
    return it == _commands.end() ? nullptr : &it->second;
}

bool Console::execute(std::string_view line) {
    if (detail::isBlankLine(line)) {
        return true;
    }
    print(OutputLevel::Echo, "> " + detail::printable(line));
    if (validateCommandLine(line)) {
        remember(line);
    }
    return run(line);
}

bool Console::run(std::string_view line) {
    if (detail::isBlankLine(line)) {
        return true;
    }
    if (auto valid = validateCommandLine(line); !valid) {
        print(OutputLevel::Error, valid.error());
        return false;
    }
    if (const std::optional<bool> raw = runRaw(line)) {
        return *raw;
    }
    auto statements = splitCommandLine(line);
    if (!statements) {
        print(OutputLevel::Error, statements.error());
        return false;
    }
    bool ok = true;
    for (const Statement& statement : *statements) {
        ok = runStatement(statement) && ok;
    }
    return ok;
}

std::optional<bool> Console::runRaw(std::string_view line) {
    const std::size_t start = line.find_first_not_of(" \t");
    const std::size_t end = std::min(line.find_first_of(" \t", start), line.size());
    const std::string name = detail::toLower(line.substr(start, end - start));
    const auto it = _commands.find(name);
    if (it == _commands.end() || !it->second.raw) {
        return std::nullopt;
    }
    std::string_view rest = line.substr(end);
    rest.remove_prefix(std::min(rest.find_first_not_of(" \t"), rest.size()));
    rest.remove_suffix(rest.size() - std::min(rest.find_last_not_of(" \t") + 1, rest.size()));
    std::vector<std::string> args;
    if (!rest.empty()) {
        args.emplace_back(rest);
    }
    return invoke(name, it->second, args);
}

bool Console::runStatement(const std::vector<std::string>& statement) {
    const std::string name = detail::toLower(statement.front());
    const std::span<const std::string> args = std::span(statement).subspan(1);

    if (const auto it = _commands.find(name); it != _commands.end()) {
        return invoke(name, it->second, args);
    }
    if (_cvars->find(name) != nullptr) {
        return runCVar(name, args);
    }
    print(OutputLevel::Error, "Unknown command or cvar '" + detail::printable(name) + "' (type 'help' for the list)");
    return false;
}

bool Console::invoke(const std::string& name, const Command& command, std::span<const std::string> args) {
    if (command.cheat && !_cvars->cheatsAllowed()) {
        print(OutputLevel::Error, "'" + name + "' is a cheat command and cheats are disabled");
        return false;
    }
    if (args.size() < command.minArgs || args.size() > command.maxArgs) {
        print(OutputLevel::Error, "Usage: " + name + (command.usage.empty() ? "" : " " + command.usage));
        return false;
    }
    CommandContext context(*this, args);
    try {
        command.handler(context);
    } catch (const std::exception& e) {
        context.error("'" + name + "' failed: " + e.what());
    }
    return !context.failed();
}

void Console::addScriptRunner(std::string_view extension, ScriptRunner runner) {
    if (!extension.starts_with('.') || extension.size() < 2) {
        throw std::invalid_argument("Console: script extension '" + std::string(extension) + "' must start with a dot");
    }
    if (!runner) {
        throw std::invalid_argument("Console: script runner for '" + std::string(extension) + "' is empty");
    }
    _scriptRunners.insert_or_assign(detail::toLower(extension), std::move(runner));
}

bool Console::removeScriptRunner(std::string_view extension) {
    const auto it = _scriptRunners.find(detail::toLower(extension));
    if (it == _scriptRunners.end()) {
        return false;
    }
    _scriptRunners.erase(it);
    return true;
}

bool Console::runScript(CommandContext& context, std::string_view path) {
    if (_scriptDepth >= MAX_SCRIPT_DEPTH) {
        context.error("Scripts nested too deep (at most " + std::to_string(MAX_SCRIPT_DEPTH) + ")");
        return false;
    }
    auto source = readScriptFile(_scriptRoot, path);
    if (!source) {
        context.error(source.error());
        return false;
    }
    // Counts this script as running until it returns, even if its runner throws.
    class DepthGuard {
      public:
        explicit DepthGuard(std::size_t& depth) noexcept : _depth(&depth) { ++*_depth; }
        ~DepthGuard() { --*_depth; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;
        DepthGuard(DepthGuard&&) = delete;
        DepthGuard& operator=(DepthGuard&&) = delete;

      private:
        std::size_t* _depth;
    };
    const DepthGuard guard(_scriptDepth);

    const std::string extension = detail::toLower(std::filesystem::path(path).extension().string());
    if (const auto runner = _scriptRunners.find(extension); runner != _scriptRunners.end()) {
        runner->second(context, *source, path);
        return !context.failed();
    }
    // No script engine for this extension: a file of console lines.
    std::size_t failures = 0;
    std::string_view rest = *source;
    while (!rest.empty()) {
        const std::size_t end = std::min(rest.find('\n'), rest.size());
        std::string_view line = rest.substr(0, end);
        rest.remove_prefix(std::min(end + 1, rest.size()));
        if (line.ends_with('\r')) {
            line.remove_suffix(1);
        }
        if (!run(line)) {
            ++failures;
        }
    }
    if (failures > 0) {
        context.error("'" + std::string(path) + "': " + std::to_string(failures) + " line(s) failed");
    }
    return failures == 0;
}

bool Console::runCVar(const std::string& name, std::span<const std::string> args) {
    if (args.empty()) {
        print(OutputLevel::Info, describeCVar(*_cvars, name));
        return true;
    }
    if (args.size() > 1) {
        print(OutputLevel::Error, "Usage: " + name + " <value> (put values with spaces in double quotes)");
        return false;
    }
    if (auto set = _cvars->setFromString(name, args.front()); !set) {
        print(OutputLevel::Error, set.error());
        return false;
    }
    print(OutputLevel::Info, name + " = " + quoteWord(CVarManager::format(_cvars->find(name)->value())));
    return true;
}

Completion Console::complete(std::string_view line) const {
    Completion completion{.line = std::string(line), .candidates = {}};
    const std::optional<detail::WordCursor> cursor = detail::locateLastWord(line);
    if (!cursor) {
        return completion;
    }
    const std::string_view typed = line.substr(cursor->start);

    std::vector<std::string> all;
    std::string prefix(typed);
    if (cursor->index == 0) {
        prefix = detail::toLower(typed);
        for (const auto& [name, command] : _commands) {
            all.push_back(name);
        }
        for (const CVar* cvar : _cvars->list()) {
            all.push_back(cvar->name());
        }
    } else {
        const std::string name = detail::toLower(cursor->name);
        if (const Command* command = findCommand(name); command != nullptr) {
            if (command->completer) {
                all = command->completer(*this, cursor->index - 1, typed);
            }
        } else if (const CVar* cvar = _cvars->find(name);
                   cvar != nullptr && cursor->index == 1 && cvar->holds<bool>()) {
            all = {"false", "true"};
        }
    }

    std::erase_if(all, [&prefix](const std::string& candidate) { return !candidate.starts_with(prefix); });
    std::ranges::sort(all);
    const auto [first, last] = std::ranges::unique(all);
    all.erase(first, last);
    if (all.empty()) {
        return completion;
    }

    const std::string_view before = line.substr(0, cursor->start);
    if (all.size() == 1) {
        completion.line = std::string(before) + quoteWord(all.front()) + " ";
    } else if (const std::string shared = detail::commonPrefix(all); shared.size() > typed.size()) {
        completion.line = std::string(before) + shared;
    }
    completion.candidates = std::move(all);
    return completion;
}

void Console::print(OutputLevel level, std::string_view text) {
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t end = std::min(text.find('\n', start), text.size());
        std::string_view piece = text.substr(start, end - start);
        if (piece.ends_with('\r')) {
            piece.remove_suffix(1);
        }
        _output.push_back(OutputLine{.level = level, .text = detail::printable(piece)});
        start = end + 1;
    }
    while (_output.size() > MAX_OUTPUT_LINES) {
        _output.pop_front();
    }
}

void Console::remember(std::string_view line) {
    if (!_history.empty() && _history.back() == line) {
        return;
    }
    _history.emplace_back(line);
    while (_history.size() > MAX_HISTORY) {
        _history.pop_front();
    }
}

std::string describeCVar(const CVarManager& cvars, std::string_view name) {
    static const std::map<CVarFlag, std::string_view> FLAG_LABELS{
        {CVarFlag::Cheat, "cheat"},
        {CVarFlag::Archived, "archived"},
        {CVarFlag::ReadOnly, "read-only"},
    };

    const CVar* cvar = cvars.find(name);
    if (cvar == nullptr) {
        return {};
    }
    std::string text = cvar->name() + " = " + quoteWord(CVarManager::format(cvar->value())) + " (" +
                       std::string(cvar->typeName()) + ", default " +
                       quoteWord(CVarManager::format(cvar->defaultValue()));
    if (const auto& range = cvar->range(); range && (cvar->holds<int>() || cvar->holds<float>())) {
        text += ", range " + CVarManager::formatNumber(range->min) + ".." + CVarManager::formatNumber(range->max);
    }
    text += ')';
    for (const auto& [flag, label] : FLAG_LABELS) {
        if (hasFlag(cvar->flags(), flag)) {
            text += " [" + std::string(label) + "]";
        }
    }
    if (!cvar->description().empty()) {
        text += " - " + cvar->description();
    }
    return text;
}

}  // namespace tools
