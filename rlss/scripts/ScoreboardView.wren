import "core/World" for World
import "rlss/components" for Camera, MatchState, ScoreCell, Transform

/**
 * @class ScoreboardView
 * @brief Client-side scoreboard drawn with small squares, until the engine can render text.
 *
 * Attached to the scoreboard entity, which carries the `MatchState` received from the server. The squares are
 * entities tagged "score_cell", each with a `ScoreCell` (display, character slot, row and column in a 3 x 5 glyph,
 * place and size on screen). This script lights the squares that draw the current text: the two scores, the kickoff
 * countdown, and "GOAL" during the celebration; display 4 (the dash between the scores) is always shown.
 *
 * The scoreboard is drawn on screen, not in the world: every frame, each square is moved and sized through the camera
 * (entity tagged "main_camera", updated before this script) so that it stays put while the camera moves.
 */
class ScoreboardView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _state = MatchState.new(_id)
        var cameras = World.getEntitiesWithTag("main_camera")
        _camera = cameras.count == 0 ? null : Camera.new(cameras[0].id)
        _cells = []
        for (tagged in World.getEntitiesWithTag("score_cell")) {
            var cell = ScoreCell.new(tagged.id)
            _cells.add(ScoreboardCell.new(Transform.new(tagged.id), cell))
        }
        _shown = null
    }

    update(dt) {
        if (_camera == null || _camera.zoom == null) return
        var blue = _state.blueScore
        if (blue == null) return
        var orange = _state.orangeScore
        var phase = _state.phase
        var countdown = _state.countdown
        var key = "%(blue)-%(orange)-%(phase)-%(countdown)"
        if (key != _shown) {
            _shown = key
            var texts = [
                ScoreboardView.twoDigits(blue),
                ScoreboardView.twoDigits(orange),
                phase == 2 && countdown > 0 ? "%(countdown)" : "",
                phase == 1 ? "GOAL" : "",
            ]
            for (cell in _cells) {
                cell.lit = cell.display == 4 || ScoreboardView.lit(texts[cell.display], cell.slot, cell.row, cell.column)
            }
        }

        // Screen to world: world = target + (screen - offset) / zoom.
        var zoom = _camera.zoom
        var targetX = _camera.targetX
        var targetY = _camera.targetY
        var offsetX = _camera.offsetX
        var offsetY = _camera.offsetY
        for (cell in _cells) {
            var transform = cell.transform
            transform.x = targetX + (cell.screenX - offsetX) / zoom
            transform.y = targetY + (cell.screenY - offsetY) / zoom
            transform.scaleX = cell.lit ? cell.width / (32 * zoom) : 0
            transform.scaleY = cell.lit ? cell.height / (32 * zoom) : 0
        }
    }

    /** @brief A score on two characters, right-aligned (" 7", "12"); 99 at most. */
    static twoDigits(value) {
        var clamped = value.min(99)
        return clamped < 10 ? " %(clamped)" : "%(clamped)"
    }

    /** @brief Whether the square (row, column) of character `slot` of `text` is lit. */
    static lit(text, slot, row, column) {
        if (slot >= text.count) return false
        var glyph = ScoreboardView.FONT[text[slot]]
        if (glyph == null) return false
        return glyph[row][column] == "#"
    }

    /** @brief 3 x 5 glyphs, one string per row, "#" for a lit square. */
    static FONT {
        if (__font == null) {
            __font = {
                "0": ["###", "#.#", "#.#", "#.#", "###"],
                "1": [".#.", "##.", ".#.", ".#.", "###"],
                "2": ["###", "..#", "###", "#..", "###"],
                "3": ["###", "..#", "###", "..#", "###"],
                "4": ["#.#", "#.#", "###", "..#", "..#"],
                "5": ["###", "#..", "###", "..#", "###"],
                "6": ["###", "#..", "###", "#.#", "###"],
                "7": ["###", "..#", "..#", ".#.", ".#."],
                "8": ["###", "#.#", "###", "#.#", "###"],
                "9": ["###", "#.#", "###", "..#", "###"],
                "G": ["###", "#..", "#.#", "#.#", "###"],
                "O": ["###", "#.#", "#.#", "#.#", "###"],
                "A": [".#.", "#.#", "###", "#.#", "#.#"],
                "L": ["#..", "#..", "#..", "#..", "###"],
            }
        }
        return __font
    }
}

/** @brief One square of the scoreboard, with its `ScoreCell` read once. */
class ScoreboardCell {
    construct new(transform, cell) {
        _transform = transform
        _display = cell.display
        _slot = cell.slot
        _row = cell.row
        _column = cell.column
        _screenX = cell.screenX
        _screenY = cell.screenY
        _width = cell.width
        _height = cell.height
        _lit = false
    }

    transform { _transform }
    display { _display }
    slot { _slot }
    row { _row }
    column { _column }
    screenX { _screenX }
    screenY { _screenY }
    width { _width }
    height { _height }
    lit { _lit }
    lit=(value) { _lit = value }
}
