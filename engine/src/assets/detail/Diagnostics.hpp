/**
 * @file Diagnostics.hpp
 * @brief Warnings of the asset manager: printed, and kept for the game and the tests to read.
 */

#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <unordered_set>

namespace assets::detail {

/** @brief Warnings kept at most; older ones are dropped, so a script looping on a bad call cannot grow memory. */
inline constexpr std::size_t MAX_KEPT_WARNINGS = 256;

/**
 * @brief Distinct keys `warnOnce` remembers at most; past it, keys already seen stay silent and new ones are warned
 * every time.
 */
inline constexpr std::size_t MAX_WARN_ONCE_KEYS = 1024;

/** @brief Collects the warnings of the asset manager. */
class Diagnostics {
  public:
    /**
     * @brief Prints a warning on the error output and keeps it.
     * @param message Warning, without the `[assets]` prefix.
     */
    void warn(std::string message);

    /**
     * @brief Warns only the first time `key` is seen, so a lookup repeated every frame or every spawn logs once.
     * @param key What the warning is about.
     * @param message Warning, without the `[assets]` prefix.
     */
    void warnOnce(std::string_view key, std::string message);

    /**
     * @brief Gets the warnings kept, oldest first.
     * @return Warnings.
     */
    [[nodiscard]] const std::deque<std::string>& warnings() const noexcept { return _warnings; }

    /** @brief Forgets the warnings kept and the keys of `warnOnce`. */
    void clear() noexcept {
        _warnings.clear();
        _onceKeys.clear();
    }

  private:
    /** @brief Warnings kept, oldest first. */
    std::deque<std::string> _warnings;
    /** @brief Hashes of the keys already reported by `warnOnce`: bounded memory whatever the key sizes. */
    std::unordered_set<std::size_t> _onceKeys;
};

}  // namespace assets::detail
