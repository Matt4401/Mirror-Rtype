/**
 * @file Diagnostics.hpp
 * @brief Warnings of the asset manager: printed, and kept for the game and the tests to read.
 */

#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace assets::detail {

/** @brief Warnings kept at most; older ones are dropped, so a script looping on a bad call cannot grow memory. */
inline constexpr std::size_t MAX_KEPT_WARNINGS = 256;

/** @brief Collects the warnings of the asset manager. */
class Diagnostics {
  public:
    /**
     * @brief Prints a warning on the error output and keeps it.
     * @param message Warning, without the `[assets]` prefix.
     */
    void warn(std::string message);

    /**
     * @brief Gets the warnings kept, oldest first.
     * @return Warnings.
     */
    [[nodiscard]] std::span<const std::string> warnings() const noexcept { return _warnings; }

    /** @brief Forgets the warnings kept. */
    void clear() noexcept { _warnings.clear(); }

  private:
    /** @brief Warnings kept, oldest first. */
    std::vector<std::string> _warnings;
};

}  // namespace assets::detail
