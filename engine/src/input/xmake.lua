target("engine_input")
    set_kind("static")
    add_files("**.cpp|InputState.cpp", "../systems/input/**.cpp")
    add_includedirs(".", {public = true})
    add_deps("engine_core", "engine_events", {public = true})
    add_packages("raylib", {public = true})
    if is_plat("windows", "mingw") then
        add_syslinks("user32", {public = true})
    end
