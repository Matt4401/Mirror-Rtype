class Entity {
    construct new(id) {
        _id = id
        _components = {}
    }

    id { _id }

    getComponent(name) { _components[name] }
    setComponent(name, comp) { _components[name] = comp }

    destroy() {
        Entity.destroyEntity(_id)
    }

    foreign static destroyEntity(id)
}
