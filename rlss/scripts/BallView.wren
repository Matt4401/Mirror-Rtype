import "core/World" for World
import "rlss/components" for Transform, BallPart, ShotGlow

/**
 * @class BallView
 * @brief Client-side look of the ball: places its pieces around the ball `Transform` every frame.
 *
 * The ball entity itself is not drawn. Its pieces are entities tagged "ball_part", with a textured `Renderable` and
 * a `BallPart` (offset from the center, base angle, whether it turns with the ball). The panels turn with the ball, so
 * its spin shows; the lighting on top never turns. After a colored shot, the halo of that color (pieces with `glow`
 * set) shows behind the ball, read from the replicated `ShotGlow`, and pulses.
 */
class BallView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _ball = Transform.new(_id)
        _glow = ShotGlow.new(_id)
        _pieces = []
        for (part in World.getEntitiesWithTag("ball_part")) {
            var info = BallPart.new(part.id)
            if (info.parent == _id) {
                var transform = Transform.new(part.id)
                _pieces.add([transform, info.offsetX, info.offsetY, info.angle, info.spins, info.glow,
                             transform.scaleX, transform.scaleY])
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
        var shot = _glow.shot
        if (shot == null) shot = 0
        _time = (_time == null ? 0 : _time) + dt
        var pulse = 1 + 0.08 * (_time * 9).sin
        for (piece in _pieces) {
            var transform = piece[0]
            if (piece[5] != 0) {
                var shown = piece[5] == shot
                transform.scaleX = shown ? piece[6] * pulse : 0
                transform.scaleY = shown ? piece[7] * pulse : 0
            }
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
