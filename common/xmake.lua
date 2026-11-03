-- Game-layer protocol: the .proto schema and the codec. Headers live next to their sources, like the engine modules.
-- Protobuf types stay private to Protocol.cpp; dependents only include protocol/Messages.hpp and protocol/Protocol.hpp.
target("common")
    set_kind("static")
    add_rules("protobuf.cpp")
    add_files("proto/*.proto")
    add_files("src/**.cpp")
    add_includedirs("src", {public = true})
    add_headerfiles("src/(protocol/*.hpp)")
    add_packages("protobuf-cpp")
