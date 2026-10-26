target("engine_ui")
    set_kind("headeronly")
    add_deps("engine_render", {public = true})
