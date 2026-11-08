#include "tools/devconsole/DevConsole.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

#include "tools/console/CommandLine.hpp"
#include "tools/console/Console.hpp"
#include "tools/console/ConsoleTypes.hpp"
#include "tools/devconsole/DevConsoleTypes.hpp"

namespace tools {

DevConsole::DevConsole(Console& console) : _console(&console), _input(MAX_LINE_LENGTH) {}

void DevConsole::type(char32_t cp) {
    if (_open && _input.insert(cp)) {
        _historyIndex.reset();
    }
}

void DevConsole::apply(EditAction action) {
    if (!_open) {
        return;
    }
    switch (action) {
        case EditAction::Submit:
            submit();
            break;
        case EditAction::Complete:
            complete();
            break;
        case EditAction::Backspace:
            _input.eraseBackward();
            break;
        case EditAction::Delete:
            _input.eraseForward();
            break;
        case EditAction::Left:
            _input.moveLeft();
            break;
        case EditAction::Right:
            _input.moveRight();
            break;
        case EditAction::Home:
            _input.moveHome();
            break;
        case EditAction::End:
            _input.moveEnd();
            break;
        case EditAction::HistoryPrevious:
            browseHistory(true);
            break;
        case EditAction::HistoryNext:
            browseHistory(false);
            break;
        case EditAction::PageUp:
            scroll(static_cast<int>(_pageSize));
            break;
        case EditAction::PageDown:
            scroll(-static_cast<int>(_pageSize));
            break;
        case EditAction::Close:
            close();
            break;
    }
}

void DevConsole::scroll(int lines) noexcept {
    if (lines >= 0) {
        _scroll = std::min(_scroll + static_cast<std::size_t>(lines), maxScroll());
    } else {
        const auto down = static_cast<std::size_t>(-static_cast<long long>(lines));
        _scroll = down >= _scroll ? 0 : _scroll - down;
    }
}

void DevConsole::setPageSize(std::size_t lines) noexcept {
    _pageSize = std::max<std::size_t>(lines, 1);
    _scroll = std::min(_scroll, maxScroll());
}

std::size_t DevConsole::maxScroll() const noexcept {
    const std::size_t total = _console->output().size();
    return total > _pageSize ? total - _pageSize : 0;
}

void DevConsole::submit() {
    const std::string line = _input.text();
    _input.clear();
    _historyIndex.reset();
    _draft.clear();
    _scroll = 0;
    (void)_console->execute(line);
}

void DevConsole::complete() {
    const Completion completion = _console->complete(_input.text().substr(0, _input.cursor()));
    if (completion.candidates.empty()) {
        return;
    }
    const std::string after = _input.text().substr(_input.cursor());
    _input.assign(completion.line);
    const std::size_t cursor = _input.cursor();
    _input.assign(completion.line + after);
    while (_input.cursor() > cursor) {
        _input.moveLeft();
    }
    if (completion.candidates.size() > 1) {
        std::string list;
        for (const std::string& candidate : completion.candidates) {
            list += (list.empty() ? "" : "  ") + candidate;
        }
        _console->print(OutputLevel::Info, list);
        _scroll = 0;
    }
}

void DevConsole::browseHistory(bool older) {
    const auto& history = _console->history();
    if (history.empty()) {
        return;
    }
    if (!_historyIndex) {
        if (!older) {
            return;
        }
        _draft = _input.text();
        _historyIndex = history.size() - 1;
    } else if (older) {
        _historyIndex = *_historyIndex == 0 ? 0 : *_historyIndex - 1;
    } else if (*_historyIndex + 1 < history.size()) {
        ++*_historyIndex;
    } else {
        _historyIndex.reset();
        _input.assign(_draft);
        return;
    }
    _input.assign(history.at(*_historyIndex));
}

}  // namespace tools
