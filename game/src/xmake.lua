-- R-Type code shared by the server and the client: replication codec and gameplay they must compute the same way.
-- Kept out of `common`, which holds the protocol alone and does not depend on the engine.
target("rtype_game")
    set_kind("static")
    add_files("game/*.cpp")
    add_includedirs(".", {public = true})
    add_deps("common", "engine_network", "log", {public = true})
