# Storage Techniques

This page analyzes the data storage and representation techniques used within the engine and the game.

---

## 1. Component Data Representation

### The Plain Old Data (POD) Approach

In our ECS architecture, components are strictly limited to **Plain Old Data (POD)** structs. They contain only raw variables (e.g., `float x`, `int hp`) and absolutely no business logic or virtual methods.

#### Justification

- **Persistence & Serialization:** Because components hold no pointers to other objects or virtual tables, an entire game state can be trivially serialized. We can easily dump the dense arrays of the `Registry` into JSON or binary files to save the game, and load them back seamlessly.
- **Reliability:** Data is kept contiguous in memory, eliminating fragmentation and memory leaks associated with complex object graphs.

---

## 2. Asset Management & Storage

### Centralized Resource Caching

Assets such as Textures, Fonts, and Sounds are inherently heavy in memory and VRAM.

#### The Problem

If ten `Enemy` entities each store a `Raylib::Texture` directly inside their `Renderable` component, the texture is loaded into VRAM ten times, resulting in massive memory bloat and slow load times.

#### The Solution

Assets are stored centrally in a `ResourceManager` or managed via ID/String hashing.
The components only store a lightweight reference (e.g., `std::string texture_id = "bydos_sprite"` or an integer hash). The Render System queries the Resource Manager using this ID, ensuring the heavy asset is only loaded from the disk into storage once.

---

## 3. Configuration and Instantiation

### Hardcoding vs. Data-Driven Definitions

Currently, entities are instantiated in code (`registry.add_component(...)`). As the project scales, having designers tweak C++ code to balance an enemy's health is counter-productive.

#### The Data-Driven Strategy

Because our components are PODs, they perfectly map to data formats like JSON. A `Prefab` storage system can parse a `.json` file containing blueprint data (e.g., `{"Health": {"hp": 100}}`) and dynamically push these components into the ECS. This guarantees clear separation between the engine's binary storage and the game's configuration storage.
