-- RLSS code shared by the server and the client that needs the engine: game components, replication codec,
-- Wren bindings. Kept out of `rlss_shared`, which holds the protocol alone and does not depend on the engine.
target("rlss_game")
    set_kind("static")
    add_files("game/*.cpp")
    add_includedirs(".", {public = true})
    add_rules("project.warnings")
    add_deps("rlss_shared", "engine_core", "engine_network", "engine_input", "engine_scripting", {public = true})
