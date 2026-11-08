#include "gameplay/ScriptBindings.hpp"

#include "LogTag.hpp"
#include "Logger.hpp"
#include "components/Transform.hpp"
#include "scripting/ComponentBinder.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace rtype::server::gameplay {

namespace {

/**
 * @brief Binds `ecs::Transform` as `Position` in one VM.
 * @param vm Wren VM.
 * @return True if the generated class was interpreted.
 */
bool bindPosition(scripting::WrenVM& vm) {
    scripting::ComponentBinder<ecs::Transform> binder(vm, POSITION_MODULE, POSITION_CLASS);
    binder.property<&ecs::Transform::x>("x").property<&ecs::Transform::y>("y");
    const auto result = vm.interpret(POSITION_MODULE, binder.generateWrenClass());
    if (!result) {
        log::error(LOG_TAG, "cannot bind ", POSITION_MODULE, " for the scripts: ", result.error());
        return false;
    }
    return true;
}

}  // namespace

bool bindScriptComponents(systems::ScriptSystem& scripts) {
    scripts.addBinderReplay([](scripting::WrenVM& vm) { (void)bindPosition(vm); });
    return bindPosition(scripts.getVm());
}

}  // namespace rtype::server::gameplay
