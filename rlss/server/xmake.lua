-- Headless authoritative server: no raylib, builds with `--graphics=n`.
target("rlss_server")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("**.cpp")
    add_includedirs(".")
    add_rules("project.warnings")
    add_deps("rlss_shared", "rlss_game", "engine_log", "engine_core", "engine_time", "engine_events", "engine_network", "engine_network_asio", "engine_scripting")
