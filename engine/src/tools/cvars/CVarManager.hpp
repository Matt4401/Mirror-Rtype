/**
 * @file CVarManager.hpp
 * @brief Registry of typed console variables (CVars): engine and game settings changed at run time by name.
 */

#pragma once

#include <cstddef>
#include <expected>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "tools/cvars/CVar.hpp"
#include "tools/cvars/CVarRef.hpp"
#include "tools/cvars/CVarTypes.hpp"

namespace tools {

/**
 * @brief Registry of the CVars of a program.
 *
 * - Code registers a variable once (`add`) and keeps the returned `CVarRef` to read it.
 * - The console changes it from text (`setFromString`, `reset`): this path checks the flags (read-only, cheat), parses
 *   strictly, checks the bounds and rejects control characters, so any typed text is safe to pass.
 * - Every actual change is recorded; `drainChanged` hands the changed names over once (the `ConsolePlugin` turns them
 *   into `CVarChanged` events).
 *
 * Cheats are disallowed by default. Not thread-safe: use it from the main thread.
 */
class CVarManager {
  public:
    /** @brief Longest accepted name. */
    static constexpr std::size_t MAX_NAME_LENGTH = 64;
    /** @brief Longest accepted string value. */
    static constexpr std::size_t MAX_STRING_LENGTH = 256;

    /** @brief Creates an empty registry. */
    CVarManager() = default;
    /** @brief Default destructor. */
    ~CVarManager() = default;
    /** @brief Not copyable: handles point into the registry. */
    CVarManager(const CVarManager&) = delete;
    /** @brief Not copyable: handles point into the registry. */
    CVarManager& operator=(const CVarManager&) = delete;
    /** @brief Not movable: handles point into the registry. */
    CVarManager(CVarManager&&) = delete;
    /** @brief Not movable: handles point into the registry. */
    CVarManager& operator=(CVarManager&&) = delete;

    /**
     * @brief Registers a variable, or returns the existing one of the same name and type (its value is kept).
     * @tparam T Type of the variable.
     * @param name Name: 1 to 64 characters among `a-z`, `0-9`, `_` and `.`, starting with a letter or `_`.
     * @param defaultValue Default value, also the starting value.
     * @param description Help text.
     * @param options Flags and bounds.
     * @return Handle on the variable.
     * @throws std::invalid_argument If the name is invalid, the default is out of bounds, or the name exists with
     *         another type.
     */
    template <CVarType T>
    CVarRef<T> add(std::string name, T defaultValue, std::string description = {}, CVarOptions options = {});

    /**
     * @brief Finds a variable.
     * @param name Name.
     * @return The variable, or `nullptr` if none has this name.
     */
    [[nodiscard]] const CVar* find(std::string_view name) const noexcept;

    /**
     * @brief Gets a typed handle on a registered variable.
     * @tparam T Type of the variable.
     * @param name Name.
     * @return The handle, or `std::nullopt` if the variable does not exist or holds another type.
     */
    template <CVarType T>
    [[nodiscard]] std::optional<CVarRef<T>> ref(std::string_view name) noexcept;

    /**
     * @brief Changes a variable from user text, checking flags, format and bounds.
     * @param name Name.
     * @param text Value as typed: `true/false/1/0/on/off` for a `bool`, a decimal number for `int` and `float`.
     * @return An error message for the user if the change is refused.
     */
    std::expected<void, std::string> setFromString(std::string_view name, std::string_view text);

    /**
     * @brief Restores the default value of a variable from the console, checking the flags.
     * @param name Name.
     * @return An error message for the user if the change is refused.
     */
    std::expected<void, std::string> reset(std::string_view name);

    /**
     * @brief Allows or disallows cheat variables. Disallowing resets every cheat variable to its default.
     * @param allowed `true` to allow console changes of cheat variables.
     */
    void setCheatsAllowed(bool allowed);

    /** @brief Tells whether cheats are allowed. @return `true` if cheat variables can be changed. */
    [[nodiscard]] bool cheatsAllowed() const noexcept { return _cheatsAllowed; }

    /**
     * @brief Lists the variables whose name starts with a prefix.
     * @param prefix Name prefix; empty for every variable.
     * @return The variables, sorted by name.
     */
    [[nodiscard]] std::vector<const CVar*> list(std::string_view prefix = {}) const;

    /** @brief Gets the number of variables. @return Number of registered variables. */
    [[nodiscard]] std::size_t size() const noexcept { return _cvars.size(); }

    /**
     * @brief Hands each changed name over once, in the order of their first change, and forgets them.
     * @param callback Called with the name of each changed variable.
     */
    void drainChanged(const std::function<void(std::string_view)>& callback);

    /**
     * @brief Tells whether a name is a valid CVar or command name.
     * @param name Candidate name.
     * @return `true` if it has 1 to 64 characters among `a-z`, `0-9`, `_`, `.` and starts with a letter or `_`.
     */
    [[nodiscard]] static bool isValidName(std::string_view name) noexcept;

    /**
     * @brief Formats a value as the console shows it (floats in their shortest exact form).
     * @param value Value.
     * @return Text of the value.
     */
    [[nodiscard]] static std::string format(const CVarValue& value);

    /**
     * @brief Formats a number in its shortest exact form (used for bounds).
     * @param number Number.
     * @return Text of the number.
     */
    [[nodiscard]] static std::string formatNumber(double number);

  private:
    friend std::expected<void, std::string> detail::assignCVar(CVarManager& manager, CVar& cvar, CVarValue value);

    /**
     * @brief Registers a variable or checks the existing one (non-template part of `add`).
     * @param candidate Variable to register.
     * @return The registered variable.
     */
    CVar& addImpl(CVar&& candidate);

    /**
     * @brief Stores a value already of the right type, after checking bounds and string content.
     * @param cvar Variable.
     * @param value New value.
     * @return An error message if the value is refused.
     */
    std::expected<void, std::string> assign(CVar& cvar, CVarValue value);

    /**
     * @brief Stores a value already validated, recording the change if the value differs.
     * @param cvar Variable.
     * @param value New value.
     */
    void store(CVar& cvar, CVarValue value);

    /**
     * @brief Finds a variable for a console change and checks its flags.
     * @param name Name.
     * @return The variable, or an error message.
     */
    std::expected<CVar*, std::string> findWritable(std::string_view name);

    /** @brief Variables by name; `std::map` keeps their address stable for the handles. */
    std::map<std::string, CVar, std::less<>> _cvars;
    /** @brief Names changed since the last `drainChanged`, without duplicates. */
    std::vector<std::string> _changed;
    /** @brief Whether cheat variables can be changed from the console. */
    bool _cheatsAllowed = false;
};

}  // namespace tools

#include "tools/cvars/CVarManager.tpp"  // IWYU pragma: export
