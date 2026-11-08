import "rlss/components" for Transform, CarParts, CarStatus

/**
 * @class CarView
 * @brief Client-side look of a car: scales and places its parts around the car `Transform` every frame.
 *
 * The car entity itself is not drawn: its `Transform` is the replicated, interpolated state (x, y, rotation, and
 * `scaleX` = -1 when facing left). Each part is an entity with a textured `Renderable` drawn around its center
 * (textures from `rlss/assets/generate_textures.py`, CarView.PX pixels per uu); this script gives it its scale and its
 * place in the car frame, mirrored when the car faces left. The boost gauge is an untextured rectangle (32 x 32 at
 * scale 1). Edit and save while the client runs: hot-reload picks the change up.
 */
class CarView {
    construct new(entity) {
        _id = entity is Num ? entity : entity.id
        _car = Transform.new(_id)
        var parts = CarParts.new(_id)
        _body = Transform.new(parts.body)
        _details = Transform.new(parts.cabin)
        _rearWheel = Transform.new(parts.rearWheel)
        _frontWheel = Transform.new(parts.frontWheel)
        _headlight = Transform.new(parts.headlight)
        _flame = Transform.new(parts.flame)
        _gaugeBack = Transform.new(parts.gaugeBack)
        _gaugeFill = Transform.new(parts.gaugeFill)
        _flash = Transform.new(parts.flash)
        _status = CarStatus.new(_id)
        _gaugeBack.scaleX = (CarView.GAUGE_WIDTH + 4) / 32
        _gaugeBack.scaleY = 9 / 32
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
        _time = _time + dt

        // Wheels roll with the distance travelled along the car (a teleport does not spin them).
        var dx = x - _lastX
        var dy = y - _lastY
        if (dx * dx + dy * dy < 200 * 200) {
            _wheelAngle = (_wheelAngle + (dx * rotation.cos + dy * rotation.sin) / CarView.WHEEL_RADIUS) % (Num.pi * 2)
        }
        _lastX = x
        _lastY = y

        // Offsets in the car frame: along the nose, then toward the roof (hitbox: 118 x 36, center at 0, 0).
        var texel = 1 / CarView.PX
        sprite_(_body, x, y, rotation, facing, 0, 0, texel, texel)
        sprite_(_details, x, y, rotation, facing, 0, 0, texel, texel)
        sprite_(_rearWheel, x, y, rotation, facing, -38, -7, texel, texel)
        sprite_(_frontWheel, x, y, rotation, facing, 38, -7, texel, texel)
        // The wheels turn the same way on screen whichever way the car faces.
        _rearWheel.rotation = rotation + _wheelAngle
        _frontWheel.rotation = rotation + _wheelAngle
        var glow = 44 / CarView.GLOW_SIZE
        sprite_(_headlight, x, y, rotation, facing, 62, 2, glow, glow * 0.7)
        updateBoost_(x, y, rotation, facing)
        updateFlash_(x, y, rotation, facing)
    }

    // After a flip reset on the ball the car blinks white, until the flip is used.
    updateFlash_(x, y, rotation, facing) {
        if (_status.flipReset == true && (_time * CarView.FLASH_RATE).sin > 0) {
            var texel = 1 / CarView.PX
            sprite_(_flash, x, y, rotation, facing, 0, 0, texel, texel)
        } else {
            hide_(_flash)
        }
    }

    // Flame behind the car while boosting, and a boost gauge above it (kept level, whatever the car rotation).
    updateBoost_(x, y, rotation, facing) {
        var boost = _status.boost
        if (boost == null) boost = 0
        if (_status.boosting == true) {
            var length = 62 + 10 * (_time * 41).sin + 6 * (_time * 67).sin
            var height = 22 + 2 * (_time * 53).sin
            var texel = 1 / CarView.PX
            sprite_(_flame, x, y, rotation, facing, -60 - length / 2, -1, texel * length / CarView.FLAME_LENGTH,
                    texel * height / CarView.FLAME_HEIGHT)
        } else {
            hide_(_flame)
        }
        var gaugeY = y - CarView.GAUGE_HEIGHT
        _gaugeBack.x = x
        _gaugeBack.y = gaugeY
        var fill = CarView.GAUGE_WIDTH * boost / 100
        _gaugeFill.scaleX = fill / 32
        _gaugeFill.scaleY = 5 / 32
        _gaugeFill.x = x - CarView.GAUGE_WIDTH / 2 + fill / 2
        _gaugeFill.y = gaugeY
    }

    /** @brief Pixels per uu of the car textures (`CAR_SCALE` in generate_textures.py). */
    static PX { 4 }
    /** @brief Radius of the drawn wheels, for their spin. */
    static WHEEL_RADIUS { 13.5 }
    /** @brief Width of glow.png, in pixels. */
    static GLOW_SIZE { 256 }
    /** @brief Size boost_flame.png is drawn for, in uu (its pixels / PX). */
    static FLAME_LENGTH { 80 }
    static FLAME_HEIGHT { 24 }
    /** @brief Blinking speed of the flip reset flash, in radians per second. */
    static FLASH_RATE { 30 }
    static GAUGE_WIDTH { 60 }
    /** @brief Height of the gauge above the car center. */
    static GAUGE_HEIGHT { 64 }

    hide_(part) {
        part.scaleX = 0
        part.scaleY = 0
    }

    // Places a sprite at (forward, up) in the car frame, mirrored when the car faces left.
    sprite_(part, x, y, rotation, facing, forward, up, scaleX, scaleY) {
        var c = rotation.cos
        var s = rotation.sin
        var along = forward * facing
        part.x = x + c * along + s * up
        part.y = y + s * along - c * up
        part.rotation = rotation
        part.scaleX = scaleX * facing
        part.scaleY = scaleY
    }
}
