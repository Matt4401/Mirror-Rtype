class Engine {
    foreign static registerWait(fiber, seconds)

    static wait(seconds) {
        registerWait(Fiber.current, seconds)
        Fiber.yield()
    }

    foreign static spawnPrefab(name, x, y)
}
