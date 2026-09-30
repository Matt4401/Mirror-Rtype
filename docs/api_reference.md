# API Reference

Welcome to the API Reference for the R-Type project.

The documentation in this section is **automatically generated** from the C++ source code using [Doxygen](https://www.doxygen.nl/index.html) and integrated via MkDocs.

By properly formatting your comments in the `.hpp` and `.cpp` files, the documentation will automatically update here when the site is built.

## How to document the C++ Code

To ensure your code appears in this API reference, use the standard Doxygen comment blocks (`/** ... */` or `///`) above your classes, structs, and functions.

### Example

```cpp
/**
 * @brief Represents a basic 2D Vector.
 *
 * This structure is used throughout the ECS for positions and velocities.
 */
struct Vector2D {
    float x; ///< The X coordinate
    float y; ///< The Y coordinate

    /**
     * @brief Adds two vectors together.
     *
     * @param other The vector to add.
     * @return A new Vector2D representing the sum.
     */
    Vector2D add(const Vector2D& other) const;
};
```

*Note: MkDoxy will automatically inject the full API tree into the site navigation.*
