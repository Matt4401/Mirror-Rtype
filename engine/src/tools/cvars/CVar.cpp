#include "tools/cvars/CVar.hpp"

#include <array>
#include <string>
#include <string_view>
#include <utility>

#include "tools/cvars/CVarTypes.hpp"

namespace tools {

CVar::CVar(std::string name, CVarValue defaultValue, std::string description, CVarOptions options)
    : _name(std::move(name)),
      _description(std::move(description)),
      _options(options),
      _default(defaultValue),
      _value(std::move(defaultValue)) {}

std::string_view CVar::typeName() const noexcept {
    static constexpr std::array<std::string_view, 4> NAMES{"bool", "int", "float", "string"};
    return NAMES.at(_value.index());
}

}  // namespace tools
