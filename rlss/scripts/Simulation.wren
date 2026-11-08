/**
 * @class Simulation
 * @brief Registry of the simulated bodies, shared by the scripts of one VM.
 *
 * Each `Car` and the `Ball` register themselves when their entity's script is created, and leave in `onDestroy()`.
 * The `Match` script walks this registry to step every body and resolve their collisions in a fixed order. A hot-reload
 * recreates the VM: the registry starts empty and the recreated instances register again.
 */
class Simulation {
    static cars {
        if (__cars == null) __cars = []
        return __cars
    }

    static addCar(car) { cars.add(car) }

    static removeCar(car) {
        var index = cars.indexOf(car)
        if (index >= 0) cars.removeAt(index)
    }

    static ball { __ball }
    static ball=(value) { __ball = value }
}
