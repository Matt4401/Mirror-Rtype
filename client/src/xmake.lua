target("r-type_client")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("**.cpp")
    add_includedirs(".")
    add_deps("engine_core", "engine_time", "engine_events", "engine_scripting", "engine_render", "engine_input", "engine_physics", "engine_audio", "engine_config", "engine_prefabs", "engine_network", "engine_network_asio", "rtype_game", "rtype_collision")
    add_packages("asio", "wren")
    if is_plat("linux") then
        -- raylib's xmake dependencies link every libxcb module, used or not. System libxcb packages may not ship these
        -- two, which would stop the binary from loading: drop them unless they are actually needed. Not add_ldflags,
        -- which xmake places after the libraries, where --as-needed has no effect.
        add_linkgroups("xcb-xevie", "xcb-ge", {as_needed = true})
    end
