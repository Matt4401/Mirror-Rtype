target("engine_time")
    set_kind("static")
    add_files("ecs/time/*.cpp")
    add_includedirs(".", {public = true})

target("engine_core")
    set_kind("static")
    add_files("ecs/*.cpp", "core/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_time")

target("engine_events")
    set_kind("headeronly")
    add_includedirs(".", {public = true})

target("engine_prefabs")
    set_kind("static")
    add_files("prefabs/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")

target("engine_scripting")
    set_kind("static")
    add_files("scripting/**.cpp", "systems/script/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events")
    add_packages("wren", {public = true})

target("engine_physics")
    set_kind("static")
    add_files("systems/physics/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")
    add_packages("box2d", {public = true})

target("engine_all")
    set_kind("phony")
    add_deps("engine_core", "engine_time", "engine_events", "engine_prefabs", "engine_scripting", "engine_physics")
    if has_config("graphics") then
        add_deps("engine_render")
    end

if has_config("graphics") then
    includes("render")
    includes("input")
end
