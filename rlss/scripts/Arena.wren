import "core/World" for World
import "rlss/components" for ArenaShape

/**
 * @class Arena
 * @brief Side view of the arena: a rectangle with quarter-circle corners, plus a goal box behind each side wall
 * (y points down).
 *
 * Like in Sideswipe, the goals open in the side walls just above the floor: the bottom curved corner is a ramp that ends
 * at the goal mouth, which goes from `goalBottom` up to `goalTop` (the crossbar) and `goalDepth` behind the wall.
 *
 * `sample(x, y)` measures the point against the walls, like a signed distance field: afterwards `distance` is how far
 * the point is inside (negative when outside), (`nx`, `ny`) is the unit normal of the closest wall, pointing inside,
 * and `curvature` is 1 / radius on a curved corner, negative around a post (a convex edge), 0 on a flat wall. Results
 * are kept in fields instead of allocated, as the physics samples several points per step.
 */
class Arena {
    construct new(left, top, right, bottom, radius, goalTop, goalBottom, goalDepth) {
        _left = left
        _top = top
        _right = right
        _bottom = bottom
        _radius = radius
        _goalTop = goalTop
        _goalBottom = goalBottom
        _goalDepth = goalDepth
        _distance = 0
        _nx = 0
        _ny = -1
        _curvature = 0
        buildOutline_()
    }

    /** @brief Builds the arena from the `ArenaShape` component of the entity tagged "arena", or null if none. */
    static fromWorld() {
        var tagged = World.getEntitiesWithTag("arena")
        if (tagged.count == 0) return null
        var shape = ArenaShape.new(tagged[0].id)
        if (shape.left == null) return null
        return Arena.new(shape.left, shape.top, shape.right, shape.bottom, shape.cornerRadius, shape.goalTop,
                         shape.goalBottom, shape.goalDepth)
    }

    left { _left }
    top { _top }
    right { _right }
    bottom { _bottom }
    radius { _radius }
    goalTop { _goalTop }
    goalBottom { _goalBottom }
    goalDepth { _goalDepth }
    hasGoals { _goalDepth > 0 && _goalBottom > _goalTop }
    centerX { (_left + _right) / 2 }
    centerY { (_top + _bottom) / 2 }
    /** @brief The four goal posts, [x, y]: the convex edges where a side wall meets a goal. */
    posts { _posts }

    distance { _distance }
    nx { _nx }
    ny { _ny }
    curvature { _curvature }

    sample(x, y) {
        // Away from the side walls the goals are too far to matter: the rounded rectangle alone is exact enough.
        if (!hasGoals || (x > _left + Arena.FAST_MARGIN && x < _right - Arena.FAST_MARGIN)) {
            sampleRoundedRect_(x, y)
        } else {
            sampleOutline_(x, y)
        }
    }

    /** @brief True if the point is inside the arena or inside a goal. */
    contains(x, y) { insideRoundedRect_(x, y) || insideGoal_(x, y) }

    static FAST_MARGIN { 400 }

    // ------------------------------------------------------------------ rounded rectangle alone

    insideRoundedRect_(x, y) {
        var dx = x - x.clamp(_left + _radius, _right - _radius)
        var dy = y - y.clamp(_top + _radius, _bottom - _radius)
        return dx * dx + dy * dy < _radius * _radius
    }

    insideGoal_(x, y) {
        if (!hasGoals || y < _goalTop || y > _goalBottom) return false
        return (x >= _left - _goalDepth && x <= _left) || (x >= _right && x <= _right + _goalDepth)
    }

    sampleRoundedRect_(x, y) {
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

    // ------------------------------------------------------------------ arena + goals

    // The outline as straight segments [ax, ay, bx, by, nx, ny] (n points inside) and corner arcs [cx, cy, sx, sy]
    // (the arc covers the points on the (sx, sy) side of its center).
    buildOutline_() {
        var l = _left
        var t = _top
        var r = _right
        var b = _bottom
        var rad = _radius
        _segments = []
        _segments.add([l + rad, t, r - rad, t, 0, 1])
        _segments.add([l + rad, b, r - rad, b, 0, -1])
        _posts = []
        if (hasGoals) {
            var d = _goalDepth
            var gt = _goalTop
            var gb = _goalBottom
            for (side in [[l, -1], [r, 1]]) {
                var wall = side[0]
                var out = side[1]
                var back = wall + out * d
                addSegment_(wall, t + rad, wall, gt, -out, 0)
                addSegment_(wall, gb, wall, b - rad, -out, 0)
                addSegment_(back.min(wall), gt, back.max(wall), gt, 0, 1)
                addSegment_(back, gt, back, gb, -out, 0)
                addSegment_(back.min(wall), gb, back.max(wall), gb, 0, -1)
                _posts.add([wall, gt])
                _posts.add([wall, gb])
            }
        } else {
            _segments.add([l, t + rad, l, b - rad, 1, 0])
            _segments.add([r, t + rad, r, b - rad, -1, 0])
        }
        _arcs = [
            [l + rad, t + rad, -1, -1],
            [r - rad, t + rad, 1, -1],
            [l + rad, b - rad, -1, 1],
            [r - rad, b - rad, 1, 1],
        ]
    }

    addSegment_(ax, ay, bx, by, nx, ny) {
        if ((bx - ax).abs + (by - ay).abs > 0.001) _segments.add([ax, ay, bx, by, nx, ny])
    }

    sampleOutline_(x, y) {
        var best = Num.largest
        var qx = x
        var qy = y
        var fallbackNx = 0
        var fallbackNy = -1
        var onArc = false
        for (s in _segments) {
            var ex = s[2] - s[0]
            var ey = s[3] - s[1]
            var along = (((x - s[0]) * ex + (y - s[1]) * ey) / (ex * ex + ey * ey)).clamp(0, 1)
            var px = s[0] + ex * along
            var py = s[1] + ey * along
            var squared = (x - px) * (x - px) + (y - py) * (y - py)
            if (squared < best) {
                best = squared
                qx = px
                qy = py
                fallbackNx = s[4]
                fallbackNy = s[5]
                onArc = false
            }
        }
        for (a in _arcs) {
            var dx = x - a[0]
            var dy = y - a[1]
            if (dx * a[2] > 0 && dy * a[3] > 0) {
                var length = (dx * dx + dy * dy).sqrt
                var gap = length - _radius
                if (gap * gap < best) {
                    best = gap * gap
                    qx = a[0] + dx / length * _radius
                    qy = a[1] + dy / length * _radius
                    fallbackNx = -dx / length
                    fallbackNy = -dy / length
                    onArc = true
                }
            }
        }

        var distance = best.sqrt
        var inside = contains(x, y)
        if (distance < 0.0001) {
            _nx = fallbackNx
            _ny = fallbackNy
        } else if (inside) {
            _nx = (x - qx) / distance
            _ny = (y - qy) / distance
        } else {
            _nx = (qx - x) / distance
            _ny = (qy - y) / distance
        }
        _distance = inside ? distance : -distance
        _curvature = 0
        if (onArc) {
            _curvature = 1 / _radius
        } else if (inside && isPost_(qx, qy)) {
            // Around a post the surface wraps a convex edge: a car going over it needs grip to stay on it.
            _curvature = -1 / distance.max(1)
        }
    }

    isPost_(x, y) {
        for (post in _posts) {
            if ((post[0] - x).abs < 0.01 && (post[1] - y).abs < 0.01) return true
        }
        return false
    }
}
