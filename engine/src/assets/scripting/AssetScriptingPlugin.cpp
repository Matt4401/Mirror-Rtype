#include "assets/scripting/AssetScriptingPlugin.hpp"

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

/** @brief Highest kind code. */
constexpr double LAST_KIND = static_cast<double>(ScriptAssetKind::Music);

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
 * @brief Reads the kind code of `core/Assets` (first argument).
 * @param call Foreign call.
 * @return The kind.
 * @throws std::invalid_argument for anything but a known code (aborts the fiber).
 */
ScriptAssetKind kindArgument(const scripting::ForeignCall& call) {
    const std::optional<double> code = call.arg<double>(1);
    if (!code || *code < 0.0 || *code > LAST_KIND || std::floor(*code) != *code) {
        throw std::invalid_argument("Assets: unknown asset kind");
    }
    return static_cast<ScriptAssetKind>(static_cast<int>(*code));
}

/**
 * @brief Calls `fn` with the asset kind tag of a script kind and its load options.
 * @param kind Script kind.
 * @param fn Callable taking a kind tag object and `LoadOptions`.
 * @return What `fn` returns.
 */
template <typename Fn>
decltype(auto) withKind(const ScriptAssetKind kind, const Fn& fn) {
    switch (kind) {
        case ScriptAssetKind::SmoothTexture:
            return fn(Texture{}, LoadOptions{.filter = TextureFilter::Smooth});
        case ScriptAssetKind::Font:
            return fn(Font{}, LoadOptions{});
        case ScriptAssetKind::Sound:
            return fn(Sound{}, LoadOptions{});
        case ScriptAssetKind::Music:
            return fn(Music{}, LoadOptions{});
        case ScriptAssetKind::Texture:
            break;
    }
    return fn(Texture{}, LoadOptions{});
}

/**
 * @brief `Assets.declare_(kind, alias, path, group)`: true if the declaration is valid.
 * @param call Foreign call.
 */
void assetsDeclare(const scripting::ForeignCall& call) {
    const ScriptAssetKind kind = kindArgument(call);
    const std::string_view alias = stringArgument(call, 2, "alias");
    const std::string_view path = stringArgument(call, 3, "path");
    const std::string_view group = stringArgument(call, 4, "group");
    AssetManager* manager = managerOf(call);
    if (manager == nullptr) {
        call.ret(false);
        return;
    }
    const bool declared = withKind(kind, [&]<typename Kind>(Kind /*tag*/, const LoadOptions& options) {
        return manager->declare<Kind>(alias, path, group, options);
    });
    call.ret(declared);
}

/**
 * @brief `Assets.preload_(kind, directory, group)`: number of files taken.
 * @param call Foreign call.
 */
void assetsPreload(const scripting::ForeignCall& call) {
    const ScriptAssetKind kind = kindArgument(call);
    const std::string_view directory = stringArgument(call, 2, "directory");
    const std::string_view group = stringArgument(call, 3, "group");
    AssetManager* manager = managerOf(call);
    const std::optional<std::string> key = normalizeAssetPath(directory);
    if (manager == nullptr || !key) {
        call.ret(0.0);
        return;
    }
    const std::size_t taken = withKind(kind, [&]<typename Kind>(Kind /*tag*/, const LoadOptions& options) {
        return manager->preloadDirectory<Kind>(*key, group, options);
    });
    call.ret(static_cast<double>(taken));
}

/**
 * @brief `Assets.id_(kind, alias)`: handle of a loaded asset, 0 otherwise.
 * @param call Foreign call.
 */
void assetsId(const scripting::ForeignCall& call) {
    const ScriptAssetKind kind = kindArgument(call);
    const std::string_view alias = stringArgument(call, 2, "alias");
    const AssetManager* manager = managerOf(call);
    if (manager == nullptr) {
        call.ret(0.0);
        return;
    }
    const double id = withKind(kind, [&]<typename Kind>(Kind /*tag*/, const LoadOptions& /*options*/) {
        return static_cast<double>(manager->get<Kind>(alias));
    });
    call.ret(id);
}

/**
 * @brief `Assets.loadGroup(group)`: true if every asset of the group is loaded.
 * @param call Foreign call.
 */
void assetsLoadGroup(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    AssetManager* manager = managerOf(call);
    call.ret(manager != nullptr && manager->loadGroup(group));
}

/**
 * @brief `Assets.unloadGroup(group)`.
 * @param call Foreign call.
 */
void assetsUnloadGroup(const scripting::ForeignCall& call) {
    const std::string_view group = stringArgument(call, 1, "group");
    if (AssetManager* manager = managerOf(call); manager != nullptr) {
        manager->unloadGroup(group);
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
    vm.registerForeignMethod<&assetsDeclare>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "declare_(_,_,_,_)");
    vm.registerForeignMethod<&assetsPreload>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "preload_(_,_,_)");
    vm.registerForeignMethod<&assetsId>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "id_(_,_)");
    vm.registerForeignMethod<&assetsLoadGroup>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "loadGroup(_)");
    vm.registerForeignMethod<&assetsUnloadGroup>(ASSETS_MODULE_NAME, ASSETS_CLASS, true, "unloadGroup(_)");
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
    scripts.addModuleSource(std::string(ASSETS_MODULE_NAME), std::string(ASSETS_MODULE_SOURCE));
    bindAssetsModule(scripts.getVm());
    scripts.addBinderReplay(&bindAssetsModule);

    _declarationsLoaded = _declarationModule.empty() || scripts.loadModule(_declarationModule);
    if (!_declarationsLoaded) {
        std::cerr << "[assets] cannot run the asset declarations of " << _declarationModule << '\n';
    }
}

}  // namespace assets
