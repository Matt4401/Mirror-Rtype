target("engine_core")
    set_kind("static")
    add_files("**.cpp|render/**.cpp|systems/render/**.cpp")
    add_includedirs(".", {public = true})

target("engine_time")
    set_kind("static")
    add_files("ecs/time/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")

target("engine_events")
    set_kind("headeronly")
    add_includedirs(".", {public = true})

target("engine_scripting")
    set_kind("static")
    add_files("scripting/**.cpp", "systems/script/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events")
    add_packages("wren", "box2d", {public = true})

target("engine_all")
    set_kind("phony")
    add_deps("engine_core", "engine_time", "engine_events", "engine_scripting")

if has_config("graphics") then
    includes("render")
end
