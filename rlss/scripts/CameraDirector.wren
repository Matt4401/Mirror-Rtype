import "rlss/components" for Camera, CameraRig, Transform

/**
 * @class CameraDirector
 * @brief Client camera, Sideswipe style: a fixed side view that frames the local car and the ball.
 *
 * Attached to the camera entity (`hasUpdate = true`). `CameraPlugin` keeps the `CameraRig` up to date (local car,
 * ball, match phase, window size, arena bounds); this script writes the engine `Camera` (target, zoom) every frame:
 *
 * 1. Side view, never rotated: only the position and the zoom change.
 * 2. It aims at a point between the car and the ball, closer to the ball when it is far away.
 * 3. The zoom keeps both on screen with a margin: close when they are close, wide when they are apart; never closer
 *    than `CLOSEST_VIEW` uu across, never wider than the arena.
 * 4. Position and zoom ease toward their target (the zoom more slowly), so bounces do not jerk the view.
 * 5. The view never goes past the arena walls: near a goal or a corner it stops against them, and the car or the ball
 *    is no longer centered.
 * 6. Kickoff: wide shot on the center. After a goal it keeps following the action until the kickoff.
 * 7. The player comes first: when the car and the ball are too far apart to fit, even fully zoomed out, the aim slides
 *    back so that the car stays on screen (the ball may leave it). The eased view is also held so that a fast car never
 *    leaves the screen while the camera catches up.
 */
class CameraDirector {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _camera = Camera.new(_id)
        _rig = CameraRig.new(_id)
        _transforms = {}
        _x = null
    }

    /** @brief Narrowest view, in uu across (about 13 car lengths). */
    static CLOSEST_VIEW { 1500 }
    /** @brief Space kept around the car and the ball, in uu. */
    static MARGIN { 280 }
    /** @brief Where the camera aims between the car (0) and the ball (1), when they are close... */
    static BALL_WEIGHT_NEAR { 0.4 }
    /** @brief ...and when they are `FAR_DISTANCE` apart or more. */
    static BALL_WEIGHT_FAR { 0.6 }
    static FAR_DISTANCE { 2000 }
    /** @brief Easing speeds, per second: the zoom follows more slowly than the position. */
    static POSITION_RATE { 5 }
    static ZOOM_RATE { 2.5 }
    /** @brief The aim keeps the car at least this far inside the edges of the screen, in uu... */
    static PLAYER_MARGIN { 320 }
    /** @brief ...and the view actually shown never lets it closer than this to an edge, even while easing. */
    static PLAYER_EDGE { 160 }
    static KICKOFF { 2 }

    update(dt) {
        var width = _rig.viewWidth
        if (width == null || width <= 0) return
        var height = _rig.viewHeight
        var left = _rig.boundsLeft
        var top = _rig.boundsTop
        var right = _rig.boundsRight
        var bottom = _rig.boundsBottom

        // Never wider than the arena, never closer than CLOSEST_VIEW.
        var widest = (width / (right - left)).max(height / (bottom - top))
        var closest = (width / CameraDirector.CLOSEST_VIEW).max(widest)

        var target = aim_(width, height, left, top, right, bottom, widest, closest)
        var zoom = target[2].clamp(widest, closest)
        var car = transform_(_rig.car)
        if (car != null) {
            // Too far from the ball to show both: keep the car on screen, show as much toward the ball as possible.
            target[0] = CameraDirector.keepInView(target[0], car.x, width / (2 * zoom), CameraDirector.PLAYER_MARGIN)
            target[1] = CameraDirector.keepInView(target[1], car.y, height / (2 * zoom), CameraDirector.PLAYER_MARGIN)
        }
        if (_x == null) {
            _x = target[0]
            _y = target[1]
            _zoom = zoom
        } else {
            var follow = 1 - (-CameraDirector.POSITION_RATE * dt).exp
            var zoomFollow = 1 - (-CameraDirector.ZOOM_RATE * dt).exp
            _x = _x + (target[0] - _x) * follow
            _y = _y + (target[1] - _y) * follow
            // Ease the zoom in log space, so zooming in and out feel the same.
            _zoom = (_zoom.log + (zoom.log - _zoom.log) * zoomFollow).exp
        }

        // The easing lags behind a fast car: never let it reach the edge of the screen.
        if (car != null) {
            _x = CameraDirector.keepInView(_x, car.x, width / (2 * _zoom), CameraDirector.PLAYER_EDGE)
            _y = CameraDirector.keepInView(_y, car.y, height / (2 * _zoom), CameraDirector.PLAYER_EDGE)
        }

        // Stay inside the arena with the zoom actually shown (the car is inside the arena: it stays in view).
        _x = CameraDirector.keepInside(_x, left, right, width / (2 * _zoom))
        _y = CameraDirector.keepInside(_y, top, bottom, height / (2 * _zoom))
        _camera.targetX = _x
        _camera.targetY = _y
        _camera.zoom = _zoom
        _camera.offsetX = width / 2
        _camera.offsetY = height / 2
    }

    // Point to aim at and zoom wanted: [x, y, zoom].
    aim_(width, height, left, top, right, bottom, widest, closest) {
        var car = transform_(_rig.car)
        var ball = transform_(_rig.ball)
        if (_rig.phase == CameraDirector.KICKOFF || (car == null && ball == null)) {
            // Wide shot on the center: both teams and the ball.
            return [(left + right) / 2, (top + bottom) / 2, widest]
        }
        if (car == null || ball == null) {
            var only = car == null ? ball : car
            return [only.x, only.y, (widest + closest) / 2]
        }
        var dx = ball.x - car.x
        var dy = ball.y - car.y
        var distance = (dx * dx + dy * dy).sqrt
        var near = CameraDirector.BALL_WEIGHT_NEAR
        var weight = near + (CameraDirector.BALL_WEIGHT_FAR - near) * (distance / CameraDirector.FAR_DISTANCE).min(1)
        var x = car.x + dx * weight
        var y = car.y + dy * weight
        var halfWidth = (car.x - x).abs.max((ball.x - x).abs) + CameraDirector.MARGIN
        var halfHeight = (car.y - y).abs.max((ball.y - y).abs) + CameraDirector.MARGIN
        return [x, y, (width / (2 * halfWidth)).min(height / (2 * halfHeight))]
    }

    transform_(id) {
        if (id == null || id < 0) return null
        if (!_transforms.containsKey(id)) _transforms[id] = Transform.new(id)
        var transform = _transforms[id]
        return transform.x == null ? null : transform
    }

    /** @brief Clamps a view center so that [center - half, center + half] stays within [low, high]. */
    /** @brief Moves a view center as little as possible so that `point` is at least `margin` inside the view. */
    static keepInView(center, point, half, margin) {
        var room = (half - margin).max(0)
        return center.clamp(point - room, point + room)
    }

    static keepInside(center, low, high, half) {
        if (high - low <= 2 * half) return (low + high) / 2
        return center.clamp(low + half, high - half)
    }
}
