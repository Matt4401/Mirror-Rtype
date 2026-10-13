#include "scripting/WrenVM.hpp"

#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include "scripting/detail/WrenTypes.hpp"

extern "C" {
#include <wren.h>
}

namespace scripting {

WrenVM::WrenVM() {
    WrenConfiguration config;
    wrenInitConfiguration(&config);

    config.userData = this;
    config.bindForeignMethodFn = &WrenVM::on_bind_foreign_method;
    config.bindForeignClassFn = &WrenVM::on_bind_foreign_class;
    config.loadModuleFn = &WrenVM::on_load_module;
    config.writeFn = &WrenVM::on_write;
    config.errorFn = &WrenVM::on_error;

    _vm = wrenNewVM(&config);
}

WrenVM::~WrenVM() {
    if (_vm != nullptr) {
        wrenFreeVM(_vm);
        _vm = nullptr;
    }
}

WrenVM::WrenVM(WrenVM&& other) noexcept
    : _vm(other._vm),
      _registry(other._registry),
      _userContext(other._userContext),
      _moduleLoader(std::move(other._moduleLoader)),
      _writeHandler(std::move(other._writeHandler)),
      _errorHandler(std::move(other._errorHandler)),
      _lastError(std::move(other._lastError)),
      _loadedSources(std::move(other._loadedSources)),
      _foreignMethods(std::move(other._foreignMethods)),
      _foreignClasses(std::move(other._foreignClasses)) {
    other._vm = nullptr;
    other._registry = nullptr;
    other._userContext = nullptr;
    if (_vm != nullptr) {
        wrenSetUserData(_vm, this);
    }
}

WrenVM& WrenVM::operator=(WrenVM&& other) noexcept {
    if (this != &other) {
        if (_vm != nullptr) {
            wrenFreeVM(_vm);
        }
        _vm = other._vm;
        _registry = other._registry;
        _userContext = other._userContext;
        _moduleLoader = std::move(other._moduleLoader);
        _writeHandler = std::move(other._writeHandler);
        _errorHandler = std::move(other._errorHandler);
        _lastError = std::move(other._lastError);
        _loadedSources = std::move(other._loadedSources);
        _foreignMethods = std::move(other._foreignMethods);
        _foreignClasses = std::move(other._foreignClasses);
        other._vm = nullptr;
        other._registry = nullptr;
        other._userContext = nullptr;
        if (_vm != nullptr) {
            wrenSetUserData(_vm, this);
        }
    }
    return *this;
}

void WrenVM::set_module_loader(ModuleLoader loader) { _moduleLoader = std::move(loader); }

void WrenVM::set_write_handler(WriteHandler handler) { _writeHandler = std::move(handler); }

void WrenVM::set_error_handler(ErrorHandler handler) { _errorHandler = std::move(handler); }

void WrenVM::register_foreign_method(std::string_view module, std::string_view className, bool isStatic,
                                     std::string_view signature, WrenForeignMethodFn fn) {
    _foreignMethods[detail::ForeignMethodKey{.module = std::string(module),
                                             .className = std::string(className),
                                             .isStatic = isStatic,
                                             .signature = std::string(signature)}] = fn;
}

void WrenVM::register_foreign_class(std::string_view module, std::string_view className,
                                    WrenForeignClassMethods methods) {
    _foreignClasses[std::string(module) + "/" + std::string(className)] = methods;
}

std::expected<void, std::string> WrenVM::interpret(std::string_view module, std::string_view source) {
    _lastError.clear();
    std::string const modStr(module);
    std::string const srcStr(source);
    WrenInterpretResult const res = wrenInterpret(_vm, modStr.c_str(), srcStr.c_str());
    if (res != WREN_RESULT_SUCCESS) {
        return std::unexpected(_lastError.empty() ? "Wren interpretation error" : _lastError);
    }
    return {};
}

detail::UniqueWrenHandle WrenVM::make_call_handle(std::string_view signature) {
    std::string const sigStr(signature);
    WrenHandle* handle = wrenMakeCallHandle(_vm, sigStr.c_str());
    return detail::UniqueWrenHandle(handle, detail::WrenHandleDeleter{.vm = _vm});
}

void WrenVM::release_handle(WrenHandle* handle) {
    if (handle != nullptr && _vm != nullptr) {
        wrenReleaseHandle(_vm, handle);
    }
}

WrenInterpretResult WrenVM::call(WrenHandle* method) { return wrenCall(_vm, method); }

WrenInterpretResult WrenVM::call(const detail::UniqueWrenHandle& method) { return call(method.get()); }

bool WrenVM::has_module(std::string_view module) {
    if (_vm == nullptr) {
        return false;
    }
    std::string const modStr(module);
    return wrenHasModule(_vm, modStr.c_str());
}

bool WrenVM::has_variable(std::string_view module, std::string_view variableName) {
    if (_vm == nullptr || !has_module(module)) {
        return false;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    return wrenHasVariable(_vm, modStr.c_str(), varStr.c_str());
}

void WrenVM::get_variable(std::string_view module, std::string_view variableName, int slot) {
    if (_vm == nullptr || !has_module(module)) {
        return;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    wrenGetVariable(_vm, modStr.c_str(), varStr.c_str(), slot);
}

void WrenVM::collect_garbage() {
    if (_vm != nullptr) {
        wrenCollectGarbage(_vm);
    }
}

void WrenVM::set_registry(void* registry) noexcept { _registry = registry; }

void* WrenVM::get_registry() const noexcept { return _registry; }

void WrenVM::set_user_context(void* ctx) noexcept { _userContext = ctx; }

void* WrenVM::get_user_context() const noexcept { return _userContext; }

::WrenVM* WrenVM::raw() noexcept { return _vm; }

void WrenVM::ensure_slots(int numSlots) { wrenEnsureSlots(_vm, numSlots); }

WrenForeignMethodFn WrenVM::on_bind_foreign_method(::WrenVM* vm, const char* module, const char* className,
                                                   bool isStatic, const char* signature) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self == nullptr) {
        return nullptr;
    }

    detail::ForeignMethodKey const key{.module = module != nullptr ? module : "",
                                       .className = className != nullptr ? className : "",
                                       .isStatic = isStatic,
                                       .signature = signature != nullptr ? signature : ""};

    auto it = self->_foreignMethods.find(key);
    if (it != self->_foreignMethods.end()) {
        return it->second;
    }
    return nullptr;
}

WrenForeignClassMethods WrenVM::on_bind_foreign_class(::WrenVM* vm, const char* module, const char* className) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self == nullptr) {
        return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
    }

    std::string const key =
        std::string(module != nullptr ? module : "") + "/" + (className != nullptr ? className : "");
    auto it = self->_foreignClasses.find(key);
    if (it != self->_foreignClasses.end()) {
        return it->second;
    }
    return WrenForeignClassMethods{.allocate = nullptr, .finalize = nullptr};
}

WrenLoadModuleResult WrenVM::on_load_module(::WrenVM* vm, const char* name) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self == nullptr || !self->_moduleLoader) {
        return WrenLoadModuleResult{.source = nullptr, .onComplete = nullptr, .userData = nullptr};
    }

    std::string const moduleName(name != nullptr ? name : "");
    auto it = self->_loadedSources.find(moduleName);
    if (it == self->_loadedSources.end()) {
        std::string source = self->_moduleLoader(moduleName);
        if (source.empty()) {
            return WrenLoadModuleResult{.source = nullptr, .onComplete = nullptr, .userData = nullptr};
        }
        it = self->_loadedSources.emplace(moduleName, std::move(source)).first;
    }

    return WrenLoadModuleResult{.source = it->second.c_str(), .onComplete = nullptr, .userData = nullptr};
}

void WrenVM::on_write(::WrenVM* vm, const char* text) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self != nullptr && self->_writeHandler) {
        self->_writeHandler(text);
    } else {
        std::cout << text;
    }
}

void WrenVM::on_error(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self == nullptr) {
        return;
    }

    std::string const modStr = (module != nullptr ? module : "?");
    std::string const msgStr = (message != nullptr ? message : "");

    if (type == WREN_ERROR_COMPILE) {
        self->_lastError += "[" + modStr + " line " + std::to_string(line) + "] " + msgStr + "\n";
    } else if (type == WREN_ERROR_RUNTIME) {
        self->_lastError += "Runtime Error: " + msgStr + "\n";
    } else if (type == WREN_ERROR_STACK_TRACE) {
        self->_lastError += "  in " + modStr + " line " + std::to_string(line) + " in " + msgStr + "\n";
    }

    if (self->_errorHandler) {
        self->_errorHandler(type, modStr, line, msgStr);
    } else {
        if (type == WREN_ERROR_COMPILE) {
            std::cerr << "[" << modStr << " line " << line << "] " << msgStr << "\n";
        } else if (type == WREN_ERROR_RUNTIME) {
            std::cerr << "Runtime Error: " << msgStr << "\n";
        } else if (type == WREN_ERROR_STACK_TRACE) {
            std::cerr << "  in " << modStr << " line " << line << " in " << msgStr << "\n";
        }
    }
}

}  // namespace scripting
