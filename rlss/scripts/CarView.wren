import "rlss/components" for Transform, CarParts, CarStatus

/**
 * @class CarView
 * @brief Client-side look of a car: sizes and places its parts around the car `Transform` every frame.
 *
 * The car entity itself is not drawn: its `Transform` is the replicated, interpolated state (x, y, rotation, and
 * `scaleX` = -1 when facing left). Each part is an entity with an untextured `Renderable` (a colored rectangle of
 * 32 x 32 at scale 1); this script gives it its size and its place in the car frame. Edit and save while the client
 * runs: hot-reload picks the change up.
 */
class CarView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _car = Transform.new(_id)
        var parts = CarParts.new(_id)
        _body = Transform.new(parts.body)
        _cabin = Transform.new(parts.cabin)
        _rearWheel = Transform.new(parts.rearWheel)
        _frontWheel = Transform.new(parts.frontWheel)
        _headlight = Transform.new(parts.headlight)
        _flame = Transform.new(parts.flame)
        _gaugeBack = Transform.new(parts.gaugeBack)
        _gaugeFill = Transform.new(parts.gaugeFill)
        _status = CarStatus.new(_id)
        resize_(_body, 118, 20)
        resize_(_cabin, 60, 18)
        resize_(_rearWheel, 22, 22)
        resize_(_frontWheel, 22, 22)
        resize_(_headlight, 7, 7)
        resize_(_gaugeBack, CarView.GAUGE_WIDTH + 4, 10)
        _time = 0
        _wheelAngle = 0
        _lastX = _car.x
        _lastY = _car.y
    }

    update(dt) {
        var x = _car.x
        if (x == null) return
        var y = _car.y
        var rotation = _car.rotation
        var facing = _car.scaleX < 0 ? -1 : 1

        // Wheels roll with the distance travelled along the car (a teleport does not spin them).
        var dx = x - _lastX
        var dy = y - _lastY
        if (dx * dx + dy * dy < 200 * 200) {
            _wheelAngle = (_wheelAngle + (dx * rotation.cos + dy * rotation.sin) / CarView.WHEEL_RADIUS) % (Num.pi * 2)
        }
        _lastX = x
        _lastY = y

        // Offsets in the car frame: along the nose, then toward the roof (hitbox: 118 x 36, center at 0, 0).
        place_(_body, x, y, rotation, facing, 0, 0, 0)
        place_(_cabin, x, y, rotation, facing, -10, 17, 0)
        place_(_rearWheel, x, y, rotation, facing, -38, -7, _wheelAngle)
        place_(_frontWheel, x, y, rotation, facing, 38, -7, _wheelAngle)
        place_(_headlight, x, y, rotation, facing, 55, 3, 0)
        updateBoost_(dt, x, y, rotation, facing)
    }

    // Flame behind the car while boosting, and a boost gauge above it (kept level, whatever the car rotation).
    updateBoost_(dt, x, y, rotation, facing) {
        _time = _time + dt
        var boost = _status.boost
        if (boost == null) boost = 0
        if (_status.boosting == true) {
            var length = 34 + 8 * (_time * 40).sin
            resize_(_flame, length, 12)
            place_(_flame, x, y, rotation, facing, -59 - length / 2, 0, 0)
        } else {
            resize_(_flame, 0, 0)
        }
        var gaugeY = y - CarView.GAUGE_HEIGHT
        _gaugeBack.x = x
        _gaugeBack.y = gaugeY
        var fill = CarView.GAUGE_WIDTH * boost / 100
        resize_(_gaugeFill, fill, 6)
        _gaugeFill.x = x - CarView.GAUGE_WIDTH / 2 + fill / 2
        _gaugeFill.y = gaugeY
    }

    static WHEEL_RADIUS { 11 }
    static GAUGE_WIDTH { 60 }
    /** @brief Height of the gauge above the car center. */
    static GAUGE_HEIGHT { 70 }

    resize_(part, width, height) {
        part.scaleX = width / 32
        part.scaleY = height / 32
    }

    place_(part, x, y, rotation, facing, forward, up, spin) {
        var c = rotation.cos
        var s = rotation.sin
        var along = forward * facing
        part.x = x + c * along + s * up
        part.y = y + s * along - c * up
        part.rotation = rotation + spin
    }
}
