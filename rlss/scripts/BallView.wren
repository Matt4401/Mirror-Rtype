import "core/World" for World
import "rlss/components" for Transform, BallPart

/**
 * @class BallView
 * @brief Client-side look of the ball: places its pieces around the ball `Transform` every frame.
 *
 * The ball entity itself is not drawn. Its pieces are entities tagged "ball_part", with an untextured `Renderable` and
 * a `BallPart` (offset from the center, base angle, whether it turns with the ball). The round body is a stack of
 * horizontal strips that never turn; the seams turn with the ball, so its spin shows.
 */
class BallView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _ball = Transform.new(_id)
        _pieces = []
        for (part in World.getEntitiesWithTag("ball_part")) {
            var info = BallPart.new(part.id)
            if (info.parent == _id) {
                _pieces.add([Transform.new(part.id), info.offsetX, info.offsetY, info.angle, info.spins])
            }
        }
    }

    update(dt) {
        var x = _ball.x
        if (x == null) return
        var y = _ball.y
        var rotation = _ball.rotation
        var c = rotation.cos
        var s = rotation.sin
        for (piece in _pieces) {
            var transform = piece[0]
            var offsetX = piece[1]
            var offsetY = piece[2]
            if (piece[4]) {
                transform.x = x + c * offsetX - s * offsetY
                transform.y = y + s * offsetX + c * offsetY
                transform.rotation = rotation + piece[3]
            } else {
                transform.x = x + offsetX
                transform.y = y + offsetY
                transform.rotation = piece[3]
            }
        }
    }
}
