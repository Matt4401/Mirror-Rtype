#include "tools/devconsole/KeyRepeater.hpp"

namespace tools {

bool KeyRepeater::update(bool down, float dt) noexcept {
    if (!down) {
        _held = false;
        return false;
    }
    if (!_held) {
        _held = true;
        _timer = _delay;
        return true;
    }
    if (!_repeats) {
        return false;
    }
    _timer -= dt;
    if (_timer > 0.0F) {
        return false;
    }
    // One press per frame at most: a long frame does not fire a burst of repeats.
    _timer = _interval;
    return true;
}

void KeyRepeater::reset(bool down) noexcept {
    _held = down;
    _timer = _delay;
}

}  // namespace tools
