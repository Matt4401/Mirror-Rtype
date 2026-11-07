import "core/World" for World
import "rlss/components" for ArenaShape

/**
 * @class Arena
 * @brief Side view of the arena: a rectangle whose four corners are quarter circles (y points down).
 *
 * `sample(x, y)` measures the point against the arena walls, like a signed distance field: afterwards `distance` is
 * how far the point is inside the arena (negative when outside), (`nx`, `ny`) is the unit normal of the closest wall,
 * pointing into the arena, and `curvature` is 1 / radius on a curved corner, 0 on a flat wall. Results are kept in
 * fields instead of allocated, as the physics samples several points per step.
 */
class Arena {
    construct new(left, top, right, bottom, radius) {
        _left = left
        _top = top
        _right = right
        _bottom = bottom
        _radius = radius
        _distance = 0
        _nx = 0
        _ny = -1
        _curvature = 0
    }

    /** @brief Builds the arena from the `ArenaShape` component of the entity tagged "arena", or null if none. */
    static fromWorld() {
        var tagged = World.getEntitiesWithTag("arena")
        if (tagged.count == 0) return null
        var shape = ArenaShape.new(tagged[0].id)
        if (shape.left == null) return null
        return Arena.new(shape.left, shape.top, shape.right, shape.bottom, shape.cornerRadius)
    }

    left { _left }
    top { _top }
    right { _right }
    bottom { _bottom }
    radius { _radius }

    distance { _distance }
    nx { _nx }
    ny { _ny }
    curvature { _curvature }

    sample(x, y) {
        // Closest point of the inner rectangle (the arena shrunk by the corner radius).
        var cx = x.clamp(_left + _radius, _right - _radius)
        var cy = y.clamp(_top + _radius, _bottom - _radius)
        var dx = x - cx
        var dy = y - cy
        if (dx != 0 && dy != 0) {
            // In a corner: the wall is the quarter circle centered on (cx, cy).
            var length = (dx * dx + dy * dy).sqrt
            _distance = _radius - length
            _nx = -dx / length
            _ny = -dy / length
            _curvature = 1 / _radius
        } else if (dx != 0) {
            _distance = _radius - dx.abs
            _nx = -dx.sign
            _ny = 0
            _curvature = 0
        } else if (dy != 0) {
            _distance = _radius - dy.abs
            _nx = 0
            _ny = -dy.sign
            _curvature = 0
        } else {
            sampleCenter_(x, y)
        }
    }

    // Far from every corner: the closest of the four flat walls.
    sampleCenter_(x, y) {
        _curvature = 0
        _distance = _bottom - y
        _nx = 0
        _ny = -1
        if (y - _top < _distance) {
            _distance = y - _top
            _ny = 1
        }
        if (x - _left < _distance) {
            _distance = x - _left
            _nx = 1
            _ny = 0
        }
        if (_right - x < _distance) {
            _distance = _right - x
            _nx = -1
            _ny = 0
        }
    }
}
