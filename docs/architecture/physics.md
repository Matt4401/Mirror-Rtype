# Physics

## Purpose

`systems::PhysicsSystem` integrates the ECS with [Box2D](https://box2d.org/). It creates and owns one Box2D body for each entity that has both `ecs::Transform` and `ecs::RigidBody2D`, advances the Box2D world at a fixed rate, and writes the resulting position, rotation, and velocity back to the ECS.

The system belongs to `engine/src/systems/physics/`. It is independent of rendering and can therefore run on both the client and the headless server.

## Components

### `ecs::Transform`

Physics uses the existing `Transform` component as its ECS-facing pose:

| Field | Unit | Physics role |
|-------|------|--------------|
| `x`, `y` | pixels | Body position |
| `rotation` | radians | Body angle |
| `scaleX`, `scaleY` | factor | Collider dimensions; negative values are treated as their absolute values. |

Before each simulation step, changes made directly to these fields reposition the Box2D body. After the step, the simulated body pose is copied back to the same fields.

### `ecs::RigidBody2D`

`RigidBody2D` enables physics for an entity when combined with `Transform`.

| Field | Default | Description |
|-------|---------|-------------|
| `bodyType` | `BodyType::Static` | Simulation mode of the Box2D body. |
| `velocityX`, `velocityY` | `0.0F` | Linear velocity in pixels per second. |
| `linearDamping`, `angularDamping` | `0.0F` | Box2D damping coefficients. |
| `maxLinearSpeed` | `2000.0F` | Maximum magnitude of the linear velocity, in pixels per second. |
| `fixedRotation` | `true` | Prevents rotation caused by physics. |
| `bullet` | `false` | Enables Box2D's continuous collision handling for fast bodies. |

`BodyType` maps directly to Box2D body types:

| Value | Behaviour |
|-------|-----------|
| `Static` | Immovable level geometry. |
| `Kinematic` | Moved by its configured velocity, unaffected by forces. |
| `Dynamic` | Fully simulated body affected by collisions and forces. |

`maxLinearSpeed` is enforced before every Box2D step. When a configured velocity exceeds the limit, `PhysicsSystem` clamps it and writes the clamped pixel-per-second values back to `RigidBody2D`.

### `ecs::Collider2D`

`Collider2D` is optional. When attached to an entity that already has `Transform` and `RigidBody2D`, it creates one Box2D fixture on that body's next update.

| Field | Default | Description |
|-------|---------|-------------|
| `shape` | `ColliderShape::Box` | `Box` or `Circle`. |
| `halfWidth`, `halfHeight` | `16.0F` | Half-extents of a box, in pixels. |
| `radius` | `16.0F` | Circle radius, in pixels. |
| `density` | `1.0F` | Fixture density. |
| `friction` | `0.2F` | Fixture friction. |
| `restitution` | `0.0F` | Fixture bounciness. |
| `categoryBits` | `0x0001` | Collision category bit mask. |
| `maskBits` | `0xFFFF` | Categories this fixture can collide with. |
| `isSensor` | `false` | Detects overlaps without physical collision response. |

For `ColliderShape::Box`, `halfWidth` and `halfHeight` are multiplied by `Transform::scaleX` and `Transform::scaleY` before being passed to Box2D as half-extents. The unused geometry fields are ignored: a circle uses `radius`, scaled by the larger absolute transform scale to remain circular, while a box uses `halfWidth` and `halfHeight`.

!!! warning "Collider changes after creation"
    The system recreates a fixture when `Transform::scaleX` or `Transform::scaleY` changes. Editing the dimensions, shape, material, filters, or sensor flag of an existing `Collider2D` does not update its existing Box2D fixture. Remove `Collider2D` for one update to destroy the fixture, then add it again to create a replacement.

## Units and Fixed Step

The ECS API uses pixels to match rendering and gameplay code. Box2D uses meters internally. `physics::PPM` is `32.0F`, so one Box2D meter equals 32 pixels.

| Value | ECS unit | Box2D unit |
|-------|----------|------------|
| Position and dimensions | pixels | meters |
| Linear velocity | pixels per second | meters per second |
| Rotation | radians | radians |

Call `update` only from a fixed-timestep simulation loop, typically using `ecs::time::Clock`. A zero or negative `fixedDt` is ignored.

```cpp
ecs::time::Clock clock;
systems::PhysicsSystem physics(registry);

while (running) {
    clock.tick(ecs::time::DEFAULT_FIXED_DT, [&](const ecs::time::Time& time) {
        physics.update(time.deltaTime);
    });
}
```

Each successful `update(fixedDt)` performs these operations in order:

1. Creates or synchronizes all `Transform` + `RigidBody2D` body pairs.
2. Creates a fixture for a newly eligible `Collider2D`.
3. Advances Box2D using 8 velocity iterations and 3 position iterations.
4. Copies each body's position, angle, and linear velocity back to its ECS components.

## Creating a Physical Entity

```cpp
#include "components/Collider2D.hpp"
#include "components/RigidBody2D.hpp"
#include "components/Transform.hpp"

const ecs::Entity player = registry.spawnEntity();

registry.emplaceComponent<ecs::Transform>(player, ecs::Transform{
    .x = 320.0F,
    .y = 180.0F,
});
registry.emplaceComponent<ecs::RigidBody2D>(player, ecs::RigidBody2D{
    .bodyType = ecs::BodyType::Dynamic,
    .velocityX = 160.0F,
    .maxLinearSpeed = 320.0F,
    .fixedRotation = true,
});
registry.emplaceComponent<ecs::Collider2D>(player, ecs::Collider2D{
    .shape = ecs::ColliderShape::Circle,
    .radius = 12.0F,
    .categoryBits = 0x0002,
    .maskBits = 0x0005,
});
```

The `PhysicsSystem` must remain alive only while its `ecs::Registry` remains alive. It registers an entity-destruction listener and automatically destroys the matching Box2D body when an entity is destroyed. `bodyCount()` can be used to inspect the number of currently managed bodies.
