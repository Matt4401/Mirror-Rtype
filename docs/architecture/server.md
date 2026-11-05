# Server Architecture

*This page should explain the Authoritative Server logic. Detail the multithreading model (I/O network threads vs. Game loop threads) and how game state updates are processed and broadcasted to clients.*

## Collisions

The server classifies physics contacts with `rtype::server::collision::CollisionRouter` (`server/src/collision/`, target `rtype_collision`). It subscribes to the engine `events::CollisionBeginEvent` and uses the categories from `prefabs/CollisionCategories.hpp` to republish each contact as an R-Type event:

| Event | Pair |
|-------|------|
| `ShotHitEvent` | Player shot ↔ enemy / obstacle (`byPlayer = true`), enemy shot ↔ player / obstacle (`byPlayer = false`) |
| `ShipContactEvent` | Player ship ↔ enemy / obstacle |
| `PowerUpTouchedEvent` | Player ship ↔ power-up |

The router does not change any state: game rules (damage, kills, pickups) subscribe to these events. It is not wired into `Room` yet. That happens once the room runs an `engine::Engine` with `PhysicsPlugin`.
