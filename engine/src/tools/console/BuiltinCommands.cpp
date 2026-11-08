#include "tools/console/BuiltinCommands.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "tools/console/CommandLine.hpp"
#include "tools/console/Console.hpp"
#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarManager.hpp"

namespace tools {

namespace {

/** @brief Names of every cvar. */
[[nodiscard]] std::vector<std::string> cvarNames(const Console& console) {
    std::vector<std::string> names;
    for (const CVar* cvar : console.cvars().list()) {
        names.push_back(cvar->name());
    }
    return names;
}

/** @brief Completer of a command whose first argument is a cvar name. */
[[nodiscard]] std::vector<std::string> completeCVarName(const Console& console, std::size_t index,
                                                        std::string_view /*prefix*/) {
    return index == 0 ? cvarNames(console) : std::vector<std::string>{};
}

/** @brief Completer of a command whose first argument is a command or cvar name. */
[[nodiscard]] std::vector<std::string> completeAnyName(const Console& console, std::size_t index,
                                                       std::string_view /*prefix*/) {
    if (index != 0) {
        return {};
    }
    std::vector<std::string> names = cvarNames(console);
    for (const auto& [name, command] : console.commands()) {
        names.push_back(name);
    }
    return names;
}

[[nodiscard]] std::string describeCommand(std::string_view name, const Command& command) {
    std::string text(name);
    if (!command.usage.empty()) {
        text += " " + command.usage;
    }
    if (!command.description.empty()) {
        text += " - " + command.description;
    }
    if (command.cheat) {
        text += " [cheat]";
    }
    return text;
}

void help(CommandContext& context) {
    const Console& console = context.console();
    if (context.args().empty()) {
        context.print("Commands:");
        for (const auto& [name, command] : console.commands()) {
            context.print("  " + describeCommand(name, command));
        }
        context.print("Type a cvar name to read it, '<cvar> <value>' to change it, 'cvar_list' to list them.");
        return;
    }
    const std::string& name = context.args().front();
    if (const Command* command = console.findCommand(name); command != nullptr) {
        context.print(describeCommand(name, *command));
    } else if (const std::string cvar = describeCVar(context.cvars(), name); !cvar.empty()) {
        context.print(cvar);
    } else {
        context.error("Unknown command or cvar '" + name + "'");
    }
}

void cvarList(const CommandContext& context) {
    const std::string_view prefix = context.args().empty() ? std::string_view{} : context.args().front();
    const std::vector<const CVar*> cvars = context.cvars().list(prefix);
    for (const CVar* cvar : cvars) {
        context.print(describeCVar(context.cvars(), cvar->name()));
    }
    context.print(std::to_string(cvars.size()) + " cvar(s)");
}

/** @brief Prints the value of a cvar after a change. */
void printValue(const CommandContext& context, const std::string& name) {
    context.print(name + " = " + quoteWord(CVarManager::format(context.cvars().find(name)->value())));
}

void set(CommandContext& context) {
    const std::string& name = context.args().front();
    if (auto done = context.cvars().setFromString(name, context.args().back()); !done) {
        context.error(done.error());
        return;
    }
    printValue(context, name);
}

void reset(CommandContext& context) {
    const std::string& name = context.args().front();
    if (auto done = context.cvars().reset(name); !done) {
        context.error(done.error());
        return;
    }
    printValue(context, name);
}

void toggle(CommandContext& context) {
    const std::string& name = context.args().front();
    const CVar* cvar = context.cvars().find(name);
    if (cvar != nullptr && !cvar->holds<bool>()) {
        context.error("'" + name + "' is a " + std::string(cvar->typeName()) + ", not a bool");
        return;
    }
    const bool current = cvar != nullptr && cvar->get<bool>();
    if (auto done = context.cvars().setFromString(name, current ? "false" : "true"); !done) {
        context.error(done.error());
        return;
    }
    printValue(context, name);
}

void echo(const CommandContext& context) {
    std::string text;
    for (const std::string& word : context.args()) {
        if (!text.empty()) {
            text += ' ';
        }
        text += word;
    }
    context.print(text);
}

}  // namespace

void registerBuiltinCommands(Console& console) {
    console.addCommand("help", {
                                   .usage = "[name]",
                                   .description = "Lists the commands, or describes a command or cvar",
                                   .handler = help,
                                   .maxArgs = 1,
                                   .completer = completeAnyName,
                               });
    console.addCommand("clear", {
                                    .description = "Empties the console output",
                                    .handler = [](CommandContext& context) { context.console().clearOutput(); },
                                    .maxArgs = 0,
                                });
    console.addCommand("echo", {
                                   .usage = "<text...>",
                                   .description = "Prints its arguments",
                                   .handler = echo,
                               });
    console.addCommand("cvar_list", {
                                        .usage = "[prefix]",
                                        .description = "Lists the cvars whose name starts with the prefix",
                                        .handler = cvarList,
                                        .maxArgs = 1,
                                        .completer = completeCVarName,
                                    });
    console.addCommand("set", {
                                  .usage = "<cvar> <value>",
                                  .description = "Changes a cvar",
                                  .handler = set,
                                  .minArgs = 2,
                                  .maxArgs = 2,
                                  .completer = completeCVarName,
                              });
    console.addCommand("reset", {
                                    .usage = "<cvar>",
                                    .description = "Restores the default value of a cvar",
                                    .handler = reset,
                                    .minArgs = 1,
                                    .maxArgs = 1,
                                    .completer = completeCVarName,
                                });
    console.addCommand("toggle", {
                                     .usage = "<cvar>",
                                     .description = "Flips a bool cvar",
                                     .handler = toggle,
                                     .minArgs = 1,
                                     .maxArgs = 1,
                                     .completer = completeCVarName,
                                 });
}

}  // namespace tools
