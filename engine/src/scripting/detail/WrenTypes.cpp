/**
 * @file WrenTypes.cpp
 * @brief Implementation of RAII deleter for Wren handles.
 */

#include "scripting/detail/WrenTypes.hpp"

#include "scripting/detail/WrenApi.hpp"

namespace scripting::detail {

void WrenHandleDeleter::operator()(::WrenHandle* handle) const noexcept {
    if (handle != nullptr && vm != nullptr) {
        wrenReleaseHandle(vm, handle);
    }
}

}  // namespace scripting::detail
