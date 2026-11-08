#include "assets/scripting/AssetScriptingPlugin.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "assets/AssetKinds.hpp"
#include "assets/AssetManager.hpp"
#include "assets/AssetPath.hpp"
#include "assets/AssetPlugin.hpp"
#include "assets/scripting/AssetScriptModule.hpp"
#include "core/Engine.hpp"
#include "core/PluginBuilder.hpp"
#include "ecs/Registry.hpp"
#include "scripting/ForeignCall.hpp"
#include "scripting/ScriptPlugin.hpp"
#include "scripting/WrenVM.hpp"
#include "systems/script/ScriptSystem.hpp"

namespace assets {

namespace {

/** @brief Name of the Wren class exposing the assets. */
constexpr std::string_view ASSETS_CLASS = "Assets";

/** @brief Most loads a script may ask for in one `loadGroupStep`. */
constexpr double MAX_STEP_LOADS = 1'000'000.0;

/**
 * @brief Gets the asset manager of the engine running a foreign call.
 * @param call Foreign call.
 * @return The manager, or null when no `AssetPlugin` runs.
 */
AssetManager* managerOf(const scripting::ForeignCall& call) {
    auto* scripts = call.userData<systems::ScriptSystem>();
    if (scripts == nullptr || scripts->getRegistry() == nullptr ||
        !scripts->getRegistry()->hasResource<AssetManager>()) {
        return nullptr;
    }
    return &scripts->getRegistry()->getResource<AssetManager>();
}

/**
 * @brief Tells whether a foreign call comes from the staging VM a hot reload validates a modified module in.
 *
 * That VM has the same bindings as the live one, but what it runs must not change anything: if the module is
 * rejected, the live declarations and loaded assets stay as they are. Mutating calls do nothing there and answer as
 * if they had succeeded, so that a script checking their result still validates.
 * @param call Foreign call.
 * @return True for a call from another VM than the script system's live one.
 */
bool isStagingCall(const scripting::ForeignCall& call) {
    auto* scripts = call.userData<systems::ScriptSystem>();
    return scripts != nullptr && call.raw() != scripts->getVm().raw();
}

/**
 * @brief Reads a string argument.
 * @param call Foreign call.
 * @param slot Argument slot.
 * @param what Name of the argument, for the error.
 * @return The string.
 * @throws std::invalid_argument if it is not a string (aborts the fiber).
 */
std::string_view stringArgument(const scripting::ForeignCall& call, const int slot, const std::string_view what) {
    const std::optional<std::string_view> value = call.arg<std::string_view>(slot);
    if (!value) {
        throw std::invalid_argument("Assets: the " + std::string(what) + " must be a string");
    }
    return *value;
}

/**
 * @brief Reads a boolean argument.
 * @param call Foreign call.
 * @param slot Argument slot.
 * @param what Name of the argument, for the error.
 * @return The boolean.
 * @throws std::invalid_argument if it is not a boolean (aborts the fiber).
 */
bool boolArgument(const scripting::ForeignCall& call, const int slot, const std::string_view what) {
    const std::optional<bool> value = call.arg<bool>(slot);
    if (!value) {
        throw std::invalid_argument("Assets: " + std::string(what) + " must be true or false");
    }
    return *value;
}

/**
 * @brief Reads the kind name (first argument) and checks the manager knows it.
 * @param call Foreign call.
 * @param manager Manager, or null.
 * @return The kind name.
 * @throws std::invalid_argument for a non-string or a kind no one registered (aborts the fiber).
 */
std::string_view kindArgument(const scripting::ForeignCall& call, const AssetManager* manager) {
    const std::string_view kind = stringArgument(call, 1, "kind");
    if (manager != nullptr && !manager->hasKind(kind)) {
        throw std::invalid_argument("Assets: unknown asset kind " + quoteForLog(kind));
    }
    return kind;
}

/**
 * @brief Normalises a path given by a script and checks it stays inside the working directory.
 * @param manager Manager, warned when the path is rejected.
 * @param path Path given by the script.
 * @return The normalised path, or nothing (warning).
 */
std::optional<std::string> confinedPath(AssetManager& manager, const std::string_view path) {
    // The working directory itself, for a directory preload.
    std::optional<std::string> normalized =
        fromUtf8(path).lexically_normal() == "." ? std::optional<std::string>(".") : normalizeAssetPath(path);
    if (!normalized) {
        manager.warn("invalid path " + quoteForLog(path));
        return std::nullopt;
    }
    if (!isConfinedAssetPath(*normalized)) {
        manager.warn("path " + quoteForLog(*normalized, MAX_PATH_LENGTH) +
                     " leaves the working directory: scripts may only name files under it");
        return std::nullopt;
    }
    return normalized;
}

/**
 * @brief Builds the load options of a script declaration.
 * @param kind Kind name: only textures are filtered, so other kinds always get the default options (one entry, not
 * two, for a sound declared "smooth").
 * @param smooth True for a smooth (mipmapped) texture.
 * @return Options.
 */
LoadOptions optionsOf(const std::string_view kind, const bool smooth) {
    const bool filtered = smooth && kind == Texture::NAME;
    return LoadOptions{.filter = filtered ? TextureFilter::Smooth : TextureFilter::Nearest};
}

/**
 * @brief Gets the Wren name of a status.
 * @param status Status.
 * @return `"unknown"`, `"declared"`, `"loaded"` or `"failed"`.
 */
std::string_view statusName(const AssetStatus status) {
    switch (status) {
        case AssetStatus::Declared:
            return "declared";
        case AssetStatus::Loaded:
            return "loaded";
        case AssetStatus::Failed:
            return "failed";
        case AssetStatus::Unknown:
            break;
    }
    return "unknown";
}

/**
 * @brief Tells whether a script may release a group; warns when not.
 * @param manager Manager.
 * @param group Group.
 * @return False for the global group, which C++ code relies on for the whole run.
 */
bool scriptMayRelease(AssetManager& manager, const std::string_view group) {
    if (group == AssetManager::GLOBAL_GROUP) {
        manager.warn("scripts cannot unload or forget the 'global' group");
        return false;
    }
    return true;
}

/**
 * @brief `Assets.declare_(kind, alias, path, group, smooth)`: true if the declaration is valid.
 * @param call Foreign call.
 */
void assetsDeclare(const scripting::ForeignCall& call) {
    AssetManager* manager = managerOf(call);
    const std::string_view kind = kindArgument(call, manager);
    const std::string_view alias = stringArgument(call, 2, "alias");
    const std::string_view path = stringArgument(call, 3, "path");
    const std::string_view group = stringArgument(call, 4, "group");
    const bool smooth = boolArgument(call, 5, "smooth");
    if (isStagingCall(call)) {
        call.ret(true);
        return;
    }
    if (manager == nullptr) {
        call.ret(false);
        return;
    }
    const std::optional<std::string> confined = confinedPath(*manager, path);
    call.ret(confined && manager->declare(kind, alias, *confined, group, optionsOf(kind, smooth)));
}

/**
 * @brief `Assets.preload_(kind, directory, group, smooth)`: number of files declared.
 * @param call Foreign call.
 */
void assetsPreload(const scripting::ForeignCall& call) {
    AssetManager* manager = managerOf(call);
    const std::string_view kind = kindArgument(call, manager);
    const std::string_view directory = stringArgument(call, 2, "directory");
    const std::string_view group = stringArgument(call, 3, "group");
    const bool smooth = boolArgument(call, 4, "smooth");
    if (manager == nullptr || isStagingCall(call)) {
        call.ret(0.0);
        return;
    }
    const std::optional<std::string> confined = confinedPath(*manager, directory);
    if (!confined) {
        call.ret(0.0);
        return;
    }
    call.ret(static_cast<double>(manager->preloadDirectory(kind, fromUtf8(*confined), group, optionsOf(kind, smooth))));
}

/**
 * @brief `Assets.undeclare(kind, alias)`: true if the alias was declared.
 * @param call Foreign call.
 */
void assetsUndeclare(const scripting::ForeignCall& call) {
    AssetManager* manager = managerOf(call);
    const std::string_view kind = kindArgument(call, manager);
    const std::string_view alias = stringArgument(call, 2, "alias");
    if (isStagingCall(call)) {
        call.ret(true);
        return;
    }
    call.ret(manager != nullptr && manager->undeclare(kind, alias));
}

/**
 * @brief `Assets.id(kind, alias)`: handle of a loaded asset, 0 otherwise.
 * @param call Foreign call.
 */
void assetsId(const scripting::ForeignCall& call) {
    const AssetManager* manager = managerOf(call);
    const std::string_view kind = kindArgument(call, manager);
    const std::string_view alias = stringArgument(call, 2, "alias");
    call.ret(manager != nullptr ? static_cast<double>(manager->handle(kind, alias)) : 0.0);
}

/**
 * @brief `Assets.status(kind, alias)`: `"unknown"`, `"declared"`, `"loaded"` or `"failed"`.
 * @param call Foreign call.
 */
void assetsStatus(const scripting::ForeignCall& call) {
    const AssetManager* manager = managerOf(call);
    const std::string_view kind = kindArgument(call, manager);
    const std::string_view alias = stringArgument(call, 2, "alias");
    call.ret(statusName(manager != nullptr ? manager->status(kind, alias) : AssetStatus::Unknown));
}

/**
 * @brief `Assets.isLoaded(alias)`: whether an asset of any kind is loaded under that alias.
 * @param call Foreign call.
 */
void assetsIsLoaded(const scripting::ForeignCall& call) {
    const std::string_view alias = stringArgument(call, 1, "alias");
    const AssetManager* manager = managerOf(call);
    bool loaded = false;
    if (manager != nullptr) {
        for (const std::string_view kind : manager->kinds()) {
            loaded = loaded || manager->status(kind, alias) == AssetStatus::Loaded;
        }
    }
    call.ret(loaded);
}

/**
 * @brief `Assets.hasKind(kind)`.
 * @param call Foreign call.
 */
void assetsHasKind(const scripting::ForeignCall& call) {
    const std::string_view kind = stringArgument(call, 1, "kind");
    const AssetManager* manager = managerOf(call);
    call.ret(manager != nullptr && manager->hasKind(kind));
}

/**
 * @brief `Assets.loadGroup(group)`: true if every asset of the group is loaded.
 * @param call Foreign call.
 */
void assetsLoadGroup(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    if (isStagingCall(call)) {
        call.ret(true);
        return;
    }
    AssetManager* manager = managerOf(call);
    call.ret(manager != nullptr && manager->loadGroup(group));
}

/**
 * @brief `Assets.loadGroupStep(group, maxLoads)`: true once nothing more can be loaded.
 * @param call Foreign call.
 */
void assetsLoadGroupStep(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    const std::optional<double> maxLoads = call.arg<double>(2);
    if (!maxLoads || !std::isfinite(*maxLoads) || *maxLoads < 0.0) {
        throw std::invalid_argument("Assets: maxLoads must be a number of at least 0");
    }
    AssetManager* manager = managerOf(call);
    if (manager == nullptr || isStagingCall(call)) {
        call.ret(true);
        return;
    }
    const auto budget = static_cast<std::size_t>(std::floor(std::min(*maxLoads, MAX_STEP_LOADS)));
    call.ret(manager->loadGroupStep(group, budget).done());
}

/**
 * @brief `Assets.groupProgress(group)`: part of the group settled, from 0 to 1.
 * @param call Foreign call.
 */
void assetsGroupProgress(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    const AssetManager* manager = managerOf(call);
    call.ret(manager != nullptr ? static_cast<double>(manager->groupProgress(group).ratio()) : 1.0);
}

/**
 * @brief `Assets.unloadGroup(group)`; refused for `"global"`.
 * @param call Foreign call.
 */
void assetsUnloadGroup(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    if (AssetManager* manager = managerOf(call);
        manager != nullptr && !isStagingCall(call) && scriptMayRelease(*manager, group)) {
        manager->unloadGroup(group);
    }
    call.retNull();
}

/**
 * @brief `Assets.forgetGroup(group)`; refused for `"global"`.
 * @param call Foreign call.
 */
void assetsForgetGroup(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    if (AssetManager* manager = managerOf(call);
        manager != nullptr && !isStagingCall(call) && scriptMayRelease(*manager, group)) {
        manager->forgetGroup(group);
    }
    call.retNull();
}

/**
 * @brief `Assets.isGroupLoaded(group)`.
 * @param call Foreign call.
 */
void assetsIsGroupLoaded(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    const AssetManager* manager = managerOf(call);
    call.ret(manager != nullptr && manager->isGroupLoaded(group));
}

/**
 * @brief Binds the foreign methods of `core/Assets`; also replayed on the VM of each hot reload.
 * @param vm VM to bind.
 */
void bindAssetsModule(scripting::WrenVM& vm) {
    vm.registerForeignMethod<&assetsDeclare>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "declare_(_,_,_,_,_)");
    vm.registerForeignMethod<&assetsPreload>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "preload_(_,_,_,_)");
    vm.registerForeignMethod<&assetsUndeclare>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "undeclare(_,_)");
    vm.registerForeignMethod<&assetsId>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "id(_,_)");
    vm.registerForeignMethod<&assetsStatus>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "status(_,_)");
    vm.registerForeignMethod<&assetsIsLoaded>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "isLoaded(_)");
    vm.registerForeignMethod<&assetsHasKind>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "hasKind(_)");
    vm.registerForeignMethod<&assetsLoadGroup>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "loadGroup(_)");
    vm.registerForeignMethod<&assetsLoadGroupStep>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "loadGroupStep(_,_)");
    vm.registerForeignMethod<&assetsGroupProgress>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "groupProgress(_)");
    vm.registerForeignMethod<&assetsUnloadGroup>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "unloadGroup(_)");
    vm.registerForeignMethod<&assetsForgetGroup>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "forgetGroup(_)");
    vm.registerForeignMethod<&assetsIsGroupLoaded>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "isGroupLoaded(_)");
}

}  // namespace

AssetScriptingPlugin::AssetScriptingPlugin(std::string declarationModule)
    : _declarationModule(std::move(declarationModule)) {}

void AssetScriptingPlugin::build(engine::PluginBuilder& builder) {
    builder.require<AssetPlugin>();
    builder.require<scripting::ScriptPlugin>();
}

void AssetScriptingPlugin::init(engine::Engine& engine) {
    systems::ScriptSystem& scripts = engine.getPlugin<scripting::ScriptPlugin>().scriptSystem();
    _scripts = &scripts;
    scripts.addModuleSource(std::string(ASSETS_MODULE_NAME), std::string(ASSETS_MODULE_SOURCE));
    bindAssetsModule(scripts.getVm());
    scripts.addBinderReplay(&bindAssetsModule);

    ecs::Registry& registry = engine.registry();
    auto& manager = registry.getResource<AssetManager>();
    // A hot reload runs every module again: the declarations they no longer make are dropped at the end.
    _resetListener = scripts.addVmResetListener([&registry, &scripts](const systems::VmResetPhase phase) {
        if (!registry.hasResource<AssetManager>()) {
            return;
        }
        auto& assets = registry.getResource<AssetManager>();
        if (phase == systems::VmResetPhase::Before) {
            assets.beginScriptDeclarations(true);
        } else {
            // A module that failed to run again did not reach all its declarations: dropping them would unload
            // assets still in use, so nothing is replaced then.
            assets.endScriptDeclarations(scripts.lastReplaySucceeded());
        }
    });

    manager.beginScriptDeclarations(false);
    _declarationsLoaded = _declarationModule.empty() || scripts.loadModule(_declarationModule);
    manager.endScriptDeclarations();
    if (!_declarationsLoaded) {
        std::cerr << "[assets] cannot run the asset declarations of " << _declarationModule << '\n';
    }
}

void AssetScriptingPlugin::shutdown(engine::Engine& /*engine*/) {
    if (_scripts != nullptr && _resetListener) {
        _scripts->removeVmResetListener(*_resetListener);
    }
    _resetListener.reset();
    _scripts = nullptr;
}

}  // namespace assets
