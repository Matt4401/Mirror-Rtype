# Physics

## Purpose

`systems::PhysicsSystem` integrates the ECS with [Box2D](https://box2d.org/). It creates and owns one Box2D body for each entity that has both `ecs::Transform` and `ecs::RigidBody2D`, advances the Box2D world at a fixed rate, and writes the resulting position, rotation, and velocity back to the ECS.

The system belongs to `engine/src/systems/physics/`. It is independent of rendering and can therefore run on both the client and the headless server.

Box2D is a private implementation detail of the `engine_physics` module: `PhysicsSystem.hpp` hides it behind a pimpl, and the `box2d` package is not exported to the targets that depend on the module. No public engine header includes Box2D, so the backend can change (for example to Box2D v3) without touching game code.

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

!!! warning "At least one Dynamic body per contact"
    Box2D only creates a contact when at least one of the two bodies is `Dynamic`. Two `Kinematic` bodies, or a `Kinematic` and a `Static` body, never collide and never produce a contact event. A body moved through its `Transform` that must detect `Kinematic` bodies (such as the R-Type player ship) should be `Dynamic` with a sensor collider: it detects contacts and the solver never pushes it.

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
systems::PhysicsSystem physics(registry, bus);

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
5. Publishes the contact events recorded during the step (see below).

## Contact Events

The system publishes two events, declared in `events/PhysicsEvents.hpp`, on the `ecs::EventBus` given to its constructor (`Engine::eventBus()` when it runs through `PhysicsPlugin`):

| Event | Published when |
|-------|----------------|
| `events::CollisionBeginEvent` | Two fixtures start touching (or overlapping, for sensors). |
| `events::CollisionEndEvent` | Two fixtures stop touching. |

These are the `CollisionEnterEvent` / `CollisionExitEvent` of issue #30. The names follow Box2D's `BeginContact` / `EndContact`.

Both carry `entityA`, `entityB`, their `categoryA`/`categoryB` (`Collider2D::categoryBits`), and `isSensor` (true when one of the two colliders is a sensor). The order of A and B is unspecified: tell them apart with the categories. Only pairs accepted by the category/mask filter produce events.

`CollisionBeginEvent` also carries the impact. It has `normalX`/`normalY` (a unit vector from A to B) and `pointX`/`pointY` (the average contact point, in pixels), and these are valid when `hasContactPoint` is true. It is false for sensor contacts, because Box2D only tests sensors for overlap and builds no contact manifold for them. `CollisionEndEvent` has no impact data, because the fixtures no longer touch.

```cpp
bus.subscribe<events::CollisionBeginEvent>([&](const events::CollisionBeginEvent& contact) {
    if ((contact.categoryA | contact.categoryB) & BULLET) {
        // ...
    }
});
```

Guarantees:

- **Published after the step.** Box2D reports contacts while the world is locked: the system buffers them and publishes once `Step` and the transform write-back are over. Subscribers may kill, spawn, or modify entities. Contacts caused by those changes are published on the next update.
- **No stale entities.** Each Box2D body has a serial number that is never reused. A contact is dropped if one of its entities was killed by an earlier subscriber, or if its id was recycled for a new entity in the meantime. If a subscriber kills an entity, that entity's other contacts from the same step are dropped.
- **No end event on destruction.** When a body is destroyed (entity killed, `RigidBody2D` removed), its contacts end silently. To react to that, listen to the registry destruction.
- **No allocation in steady state.** The listener (`systems/physics/detail/Box2DContactListener`, private to the module) records plain structs into a reserved vector (`physics::INITIAL_CONTACT_CAPACITY`). The system swaps it with a second reserved vector before publishing, so neither buffer is reallocated once it reaches its high-water mark. `test_physics` checks that a warmed-up update with contacts makes no heap allocation.
- **Scripts.** `ScriptPlugin` forwards `CollisionBeginEvent` to `onCollision(other)` on both entities' scripts, if they set `ScriptComponent::hasCollision` (see the [scripting guide](../getting-started/scripting-guide.md)).
- Removing a `Collider2D`, or changing the transform scale (which rebuilds the fixture), ends the fixture's contacts normally. A scale change can therefore produce an end/begin pair.

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
