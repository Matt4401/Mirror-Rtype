-- Code shared by the RLSS server and client: protocol constants, message codec, command-line helpers.
-- Independent from the engine, like R-Type's `common`.
target("rlss_shared")
    set_kind("static")
    add_files("**.cpp")
    add_includedirs(".", {public = true})
    add_rules("project.warnings")
