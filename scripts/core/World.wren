import "core/Entity" for Entity

class World {
    foreign static idsWithTag_(tag)
    foreign static nearestIdWithTag_(x, y, tag)
    foreign static idsInRadius_(x, y, radius)

    static getEntitiesWithTag(tag) { wrap_(idsWithTag_(tag)) }

    static getNearestWithTag(x, y, tag) {
        var id = nearestIdWithTag_(x, y, tag)
        return id == null ? null : Entity.new(id)
    }

    static getEntitiesInRadius(x, y, radius) { wrap_(idsInRadius_(x, y, radius)) }

    static wrap_(ids) {
        for (i in 0...ids.count) ids[i] = Entity.new(ids[i])
        return ids
    }
}
