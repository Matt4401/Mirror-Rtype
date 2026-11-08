/**
 * @file DevConsoleView.hpp
 * @brief Draws the developer console through the screen draw service.
 */

#pragma once

#include <cstddef>
#include <utility>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/devconsole/DevConsoleTypes.hpp"

namespace tools {

class DevConsole;

/**
 * @brief Draws the developer console: a panel across the top of the window with the output log (newest line at the
 * bottom, coloured by kind), a scroll hint, and the input line with its cursor.
 *
 * Draws through `engine::IScreenDrawService` only, so it holds no graphics-library code.
 */
class DevConsoleView {
  public:
    /**
     * @brief Creates a view.
     * @param config Layout, font and colours.
     */
    explicit DevConsoleView(DevConsoleConfig config) noexcept : _config(std::move(config)) {}

    /**
     * @brief Sets the font of the text.
     * @param font Font loaded through the screen draw service, or `engine::NO_FONT` for the default one.
     */
    void setFont(engine::FontId font) noexcept { _font = font; }

    /**
     * @brief Computes how many output lines fit in the panel.
     * @param screenHeight Window height in pixels.
     * @return Visible output lines, at least 1.
     */
    [[nodiscard]] std::size_t visibleLines(float screenHeight) const noexcept;

    /**
     * @brief Queues the console drawing.
     * @param screen Screen draw service.
     * @param console Console state.
     * @param screenWidth Window width in pixels.
     * @param screenHeight Window height in pixels.
     * @param caretVisible Whether the cursor is shown (it blinks).
     */
    void draw(engine::IScreenDrawService& screen, const DevConsole& console, float screenWidth, float screenHeight,
              bool caretVisible) const;

  private:
    /** @brief Height of a text line, spacing included. */
    [[nodiscard]] float lineHeight() const noexcept { return _config.fontSize + _config.lineSpacing; }

    /** @brief Height of the panel for a window height. */
    [[nodiscard]] float panelHeight(float screenHeight) const noexcept;

    /** @brief Colour of an output line. */
    [[nodiscard]] const ecs::Rgba& colorOf(OutputLevel level) const noexcept;

    /** @brief Layout, font and colours. */
    DevConsoleConfig _config;
    /** @brief Font of the text. */
    engine::FontId _font = engine::NO_FONT;
};

}  // namespace tools
