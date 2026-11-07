-- No graphics library here: drawing goes through engine::IScreenDrawService, so the module also builds headless.
target("engine_ui")
    set_kind("static")
    add_files("**.cpp")
    add_deps("engine_core", {public = true})
    add_deps("engine_hierarchy")
