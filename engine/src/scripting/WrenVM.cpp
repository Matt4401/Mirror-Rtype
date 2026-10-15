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
    config.bindForeignMethodFn = &WrenVM::onBindForeignMethod;
    config.bindForeignClassFn = &WrenVM::onBindForeignClass;
    config.loadModuleFn = &WrenVM::onLoadModule;
    config.writeFn = &WrenVM::onWrite;
    config.errorFn = &WrenVM::onError;

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

void WrenVM::setModuleLoader(ModuleLoader loader) { _moduleLoader = std::move(loader); }

const WrenVM::ModuleLoader& WrenVM::getModuleLoader() const noexcept { return _moduleLoader; }

void WrenVM::reset() {
    if (_vm != nullptr) {
        wrenFreeVM(_vm);
        _vm = nullptr;
    }
    _loadedSources.clear();
    _lastError.clear();

    WrenConfiguration config;
    wrenInitConfiguration(&config);

    config.userData = this;
    config.bindForeignMethodFn = &WrenVM::onBindForeignMethod;
    config.bindForeignClassFn = &WrenVM::onBindForeignClass;
    config.loadModuleFn = &WrenVM::onLoadModule;
    config.writeFn = &WrenVM::onWrite;
    config.errorFn = &WrenVM::onError;

    _vm = wrenNewVM(&config);
}

void WrenVM::setWriteHandler(WriteHandler handler) { _writeHandler = std::move(handler); }

void WrenVM::setErrorHandler(ErrorHandler handler) { _errorHandler = std::move(handler); }

void WrenVM::registerForeignMethod(std::string_view module, std::string_view className, bool isStatic,
                                   std::string_view signature, WrenForeignMethodFn fn) {
    _foreignMethods[detail::ForeignMethodKey{.module = std::string(module),
                                             .className = std::string(className),
                                             .isStatic = isStatic,
                                             .signature = std::string(signature)}] = fn;
}

void WrenVM::registerForeignClass(std::string_view module, std::string_view className,
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

detail::UniqueWrenHandle WrenVM::makeCallHandle(std::string_view signature) {
    std::string const sigStr(signature);
    WrenHandle* handle = wrenMakeCallHandle(_vm, sigStr.c_str());
    return detail::UniqueWrenHandle(handle, detail::WrenHandleDeleter{.vm = _vm});
}

void WrenVM::releaseHandle(WrenHandle* handle) {
    if (handle != nullptr && _vm != nullptr) {
        wrenReleaseHandle(_vm, handle);
    }
}

WrenInterpretResult WrenVM::call(WrenHandle* method) { return wrenCall(_vm, method); }

WrenInterpretResult WrenVM::call(const detail::UniqueWrenHandle& method) { return call(method.get()); }

bool WrenVM::hasModule(std::string_view module) {
    if (_vm == nullptr) {
        return false;
    }
    std::string const modStr(module);
    return wrenHasModule(_vm, modStr.c_str());
}

bool WrenVM::hasVariable(std::string_view module, std::string_view variableName) {
    if (_vm == nullptr || !hasModule(module)) {
        return false;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    return wrenHasVariable(_vm, modStr.c_str(), varStr.c_str());
}

void WrenVM::getVariable(std::string_view module, std::string_view variableName, int slot) {
    if (_vm == nullptr || !hasModule(module)) {
        return;
    }
    std::string const modStr(module);
    std::string const varStr(variableName);
    wrenGetVariable(_vm, modStr.c_str(), varStr.c_str(), slot);
}

void WrenVM::collectGarbage() {
    if (_vm != nullptr) {
        wrenCollectGarbage(_vm);
    }
}

void WrenVM::setRegistry(void* registry) noexcept { _registry = registry; }

void* WrenVM::getRegistry() const noexcept { return _registry; }

void WrenVM::setUserContext(void* ctx) noexcept { _userContext = ctx; }

void* WrenVM::getUserContext() const noexcept { return _userContext; }

::WrenVM* WrenVM::raw() noexcept { return _vm; }

void WrenVM::ensureSlots(int numSlots) { wrenEnsureSlots(_vm, numSlots); }

WrenForeignMethodFn WrenVM::onBindForeignMethod(::WrenVM* vm, const char* module, const char* className, bool isStatic,
                                                const char* signature) {
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

WrenForeignClassMethods WrenVM::onBindForeignClass(::WrenVM* vm, const char* module, const char* className) {
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

WrenLoadModuleResult WrenVM::onLoadModule(::WrenVM* vm, const char* name) {
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

void WrenVM::onWrite(::WrenVM* vm, const char* text) {
    auto* self = static_cast<WrenVM*>(wrenGetUserData(vm));
    if (self != nullptr && self->_writeHandler) {
        self->_writeHandler(text);
    } else {
        std::cout << text;
    }
}

void WrenVM::onError(::WrenVM* vm, WrenErrorType type, const char* module, int line, const char* message) {
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
