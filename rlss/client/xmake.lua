target("rlss_client")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("**.cpp")
    add_includedirs(".")
    add_rules("project.warnings")
    add_deps("rlss_shared", "rlss_game", "log", "engine_core", "engine_time", "engine_events", "engine_render",
             "engine_audio", "engine_input", "engine_network", "engine_network_asio", "engine_scripting",
             "engine_hierarchy", "engine_ui", "engine_scene", "engine_config", "engine_settings", "engine_assets",
             "engine_assets_scripting", "engine_assets_render", "engine_assets_audio", "engine_assets_scene")
