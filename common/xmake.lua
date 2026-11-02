-- Game-layer protocol: the .proto schema and the codec. Protobuf types stay private to src/;
-- dependents only include protocol/*.hpp.
target("common")
    set_kind("static")
    add_rules("protobuf.cpp")
    add_files("proto/*.proto")
    add_files("src/**.cpp")
    add_includedirs("include", {public = true})
    add_headerfiles("include/(**.hpp)")
    add_packages("protobuf-cpp")
