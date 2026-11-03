target("engine_input")
    set_kind("static")
    add_files("**.cpp|InputState.cpp", "../systems/input/**.cpp")
    add_includedirs(".", {public = true})
    -- InputState.cpp is built by engine_input_state (raylib-free, shared with headless targets).
    add_deps("engine_core", "engine_events", "engine_input_state", {public = true})
    add_packages("raylib", {public = true})
    if is_plat("windows", "mingw") then
        add_syslinks("user32", {public = true})
    end
