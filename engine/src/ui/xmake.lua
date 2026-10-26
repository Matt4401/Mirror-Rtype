target("engine_ui")
    set_kind("static")
    add_files("**.cpp")
    add_deps("engine_render", {public = true})
