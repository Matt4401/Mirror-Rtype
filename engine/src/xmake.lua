target("engine_time")
    set_kind("static")
    add_files("ecs/time/*.cpp")
    add_includedirs(".", {public = true})
    -- PreciseSleepScope asks Windows for a 1 ms timer period (timeBeginPeriod).
    if is_plat("windows", "mingw") then
        add_syslinks("winmm", {public = true})
    end

target("engine_core")
    set_kind("static")
    add_files("ecs/*.cpp", "core/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_time")

target("engine_events")
    set_kind("headeronly")
    add_includedirs(".", {public = true})

target("engine_config")
    set_kind("static")
    add_files("config/*.cpp")
    add_includedirs(".", {public = true})
    add_packages("toml++")

target("engine_prefabs")
    set_kind("static")
    add_files("prefabs/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")

-- Measures and the board holding them: no graphics library, so a headless program can use them too.
target("engine_metrics")
    set_kind("static")
    add_files("metrics/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")
    if is_plat("windows", "mingw") then
        add_syslinks("psapi", {public = true})
    end

target("engine_scripting")
    set_kind("static")
    add_files("scripting/**.cpp", "systems/script/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events", "engine_input", "engine_prefabs")
    add_packages("wren", {public = true})

-- Network interfaces (transport, config) and replication building blocks (input queue, network id map).
-- No socket library here: code that only talks to the interfaces never depends on asio.
target("engine_network")
    set_kind("static")
    add_files("net/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")

-- Network lines of the metrics board. Apart from engine_network, so that the network does not depend on the metrics.
target("engine_network_metrics")
    set_kind("static")
    add_files("net/metrics/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_network", "engine_metrics")

-- Asio implementation of the transport, behind net/NetworkFactory.hpp. Only the executables that open sockets link it.
target("engine_network_asio")
    set_kind("static")
    add_files("net/asio/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_network", "log")
    add_packages("asio")
    if is_plat("windows", "mingw") then
        add_syslinks("ws2_32", "mswsock", {public = true})
    end

target("engine_physics")
    set_kind("static")
    add_files("systems/physics/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events")
    add_packages("box2d")

target("engine_hierarchy")
    set_kind("static")
    add_files("hierarchy/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core")

-- HUD panel showing the metrics board. Draws through engine_ui, which holds no graphics-library code either.
target("engine_metrics_overlay")
    set_kind("static")
    add_files("metrics/overlay/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_metrics", "engine_ui", "engine_hierarchy")

-- Scenes: stack of game states owning their entities, subscriptions and systems. Headless, no backend dependency.
target("engine_scene")
    set_kind("static")
    add_files("scene/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events")

target("engine_all")
    set_kind("phony")
    add_deps("engine_core", "engine_time", "engine_events", "engine_config", "engine_prefabs", "engine_scripting", "engine_network", "engine_network_metrics", "engine_network_asio", "engine_physics", "engine_input", "engine_hierarchy", "engine_ui", "engine_metrics", "engine_metrics_overlay", "engine_scene")
    if has_config("graphics") then
        add_deps("engine_render", "engine_audio")
    end

if has_config("graphics") then
    includes("render")
    includes("audio")
end
includes("ui")
includes("input")
