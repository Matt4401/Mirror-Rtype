#include "tools/devconsole/DevConsoleView.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

#include "components/Renderable.hpp"
#include "core/services/IScreenDrawService.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/devconsole/DevConsole.hpp"

namespace tools {

float DevConsoleView::panelHeight(float screenHeight) const noexcept {
    const float minimum = (2.0F * _config.padding) + (3.0F * lineHeight());
    return std::min(std::max(screenHeight * _config.heightRatio, minimum), screenHeight);
}

std::size_t DevConsoleView::visibleLines(float screenHeight) const noexcept {
    // Panel = padding, output rows, separator gap, input row, padding.
    const float outputHeight = panelHeight(screenHeight) - (2.0F * _config.padding) - lineHeight();
    const float rows = std::floor(outputHeight / lineHeight());
    return rows >= 1.0F ? static_cast<std::size_t>(rows) : 1;
}

const ecs::Rgba& DevConsoleView::colorOf(OutputLevel level) const noexcept {
    switch (level) {
        case OutputLevel::Echo:
            return _config.echoColor;
        case OutputLevel::Error:
            return _config.errorColor;
        case OutputLevel::Info:
            break;
    }
    return _config.textColor;
}

void DevConsoleView::draw(engine::IScreenDrawService& screen, const DevConsole& console, float screenWidth,
                          float screenHeight, bool caretVisible) const {
    const float panel = panelHeight(screenHeight);
    const float pad = _config.padding;
    const float size = _config.fontSize;
    const float spacing = _config.letterSpacing;
    const float inputY = panel - pad - size;
    const float separatorY = inputY - (_config.lineSpacing * 0.5F) - 1.0F;

    screen.fillRect({.x = 0.0F, .y = 0.0F, .width = screenWidth, .height = panel}, _config.background);
    screen.fillRect({.x = 0.0F, .y = panel - 2.0F, .width = screenWidth, .height = 2.0F}, _config.border);
    screen.fillRect({.x = 0.0F, .y = separatorY, .width = screenWidth, .height = 1.0F}, _config.border);

    // Output, newest line just above the separator.
    const auto& output = console.console().output();
    const std::size_t rows = visibleLines(screenHeight);
    const std::size_t hidden = std::min(console.scrollOffset(), output.size());
    const std::size_t shown = std::min(rows, output.size() - hidden);
    for (std::size_t row = 0; row < shown; ++row) {
        const OutputLine& line = output.at(output.size() - hidden - 1 - row);
        const float y = separatorY - (static_cast<float>(row + 1) * lineHeight());
        screen.drawText(line.text, pad, y, size, spacing, colorOf(line.level), _font);
    }

    // Input line and cursor.
    const std::string prompt = "> ";
    const std::string& text = console.input().text();
    screen.drawText(prompt + text, pad, inputY, size, spacing, _config.textColor, _font);
    if (caretVisible) {
        const std::string beforeCursor = prompt + text.substr(0, console.input().cursor());
        const float caretX = pad + screen.measureText(beforeCursor, size, spacing, _font).width;
        screen.fillRect({.x = caretX, .y = inputY, .width = 2.0F, .height = size}, _config.textColor);
    }

    if (hidden > 0) {
        const std::string hint = "[" + std::to_string(hidden) + " newer line(s) below]";
        const float hintWidth = screen.measureText(hint, size, spacing, _font).width;
        screen.drawText(hint, screenWidth - pad - hintWidth, inputY, size, spacing, _config.hintColor, _font);
    }
}

}  // namespace tools
