#include "tools/scripting/ConsoleScriptingPlugin.hpp"

#include <memory>
#include <string>
#include <string_view>

#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "tools/console/CommandContext.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsolePlugin.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/cvars/CVarManager.hpp"
#include "tools/scripting/WrenConsole.hpp"

namespace tools {

void ConsoleScriptingPlugin::build(engine::PluginBuilder& builder) {
    builder.require<ConsolePlugin>();
    builder.require<scripting::ScriptPlugin>();
}

void ConsoleScriptingPlugin::init(engine::Engine& engine) {
    _wren = std::make_unique<WrenConsole>(engine.getPlugin<scripting::ScriptPlugin>().scriptSystem().getVm());
    auto& console = engine.service<Console>();

    console.addCommand(
        std::string(COMMAND),
        {
            .usage = "<code>",
            .description = "Runs Wren code in the 'console' module; an expression prints its value",
            .handler = [this](CommandContext& context) { (void)_wren->evaluate(context.args().front(), context); },
            .minArgs = 1,
            .maxArgs = 1,
            .cheat = true,
            .raw = true,
        });
    console.addScriptRunner(EXTENSION, [this](CommandContext& context, std::string_view source, std::string_view path) {
        if (!context.cvars().cheatsAllowed()) {
            context.error("'" + std::string(path) + "': running Wren files is a cheat and cheats are disabled");
            return;
        }
        (void)_wren->runFile(source, path, context);
    });
}

void ConsoleScriptingPlugin::shutdown(engine::Engine& engine) {
    if (auto console = engine.mediator().getService<Console>(); console) {
        (void)console->get().removeCommand(COMMAND);
        (void)console->get().removeScriptRunner(EXTENSION);
    }
    _wren.reset();
}

}  // namespace tools
