import "core/World" for World
import "rlss/components" for MatchState, ScoreCell, Transform

/**
 * @class ScoreboardView
 * @brief Client-side scoreboard drawn with small squares, until the engine can render text.
 *
 * Attached to the scoreboard entity, which carries the `MatchState` received from the server. The squares are
 * entities tagged "score_cell", each with a `ScoreCell` (display, character slot, row and column in a 3 x 5 glyph,
 * size). This script lights the squares that draw the current text: the two scores, the kickoff countdown, and "GOAL"
 * during the celebration. A square is lit by giving it its size, hidden by giving it a zero size.
 */
class ScoreboardView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _state = MatchState.new(_id)
        _cells = []
        for (tagged in World.getEntitiesWithTag("score_cell")) {
            var cell = ScoreCell.new(tagged.id)
            _cells.add([Transform.new(tagged.id), cell.display, cell.slot, cell.row, cell.column, cell.size / 32])
        }
        _shown = null
    }

    update(dt) {
        var blue = _state.blueScore
        if (blue == null) return
        var orange = _state.orangeScore
        var phase = _state.phase
        var countdown = _state.countdown
        var key = "%(blue)-%(orange)-%(phase)-%(countdown)"
        if (key == _shown) return
        _shown = key

        var texts = [
            ScoreboardView.twoDigits(blue),
            ScoreboardView.twoDigits(orange),
            phase == 2 && countdown > 0 ? "%(countdown)" : "",
            phase == 1 ? "GOAL" : "",
        ]
        for (cell in _cells) {
            var scale = ScoreboardView.lit(texts[cell[1]], cell[2], cell[3], cell[4]) ? cell[5] : 0
            cell[0].scaleX = scale
            cell[0].scaleY = scale
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
