#include "tools/console/Console.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
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
    auto statements = splitCommandLine(line);
    if (!statements) {
        print(OutputLevel::Error, statements.error());
        return false;
    }
    remember(line);
    bool ok = true;
    for (const Statement& statement : *statements) {
        ok = run(statement) && ok;
    }
    return ok;
}

bool Console::run(const std::vector<std::string>& statement) {
    const std::string name = detail::toLower(statement.front());
    const std::span<const std::string> args = std::span(statement).subspan(1);

    if (const auto it = _commands.find(name); it != _commands.end()) {
        const Command& command = it->second;
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
    if (_cvars->find(name) != nullptr) {
        return runCVar(name, args);
    }
    print(OutputLevel::Error, "Unknown command or cvar '" + detail::printable(name) + "' (type 'help' for the list)");
    return false;
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
