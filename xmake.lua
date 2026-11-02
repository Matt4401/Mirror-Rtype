set_project("R-type")
set_version("0.1.0")
set_languages("c++23")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("asio", "gtest", "wren", "box2d v2.4.2", "toml++ v3.4.0", "protobuf-cpp 31.1")
-- protobuf picks its C++ standard from abseil (default 17). The type of protobuf's global empty string differs
-- between C++17 and C++20, which MSVC encodes in the mangled name: build both in C++20 to match our c++23 code.
add_requireconfs("protobuf-cpp.abseil", {configs = {cxx_standard = "20"}})

if is_plat("windows") then
    add_defines("_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

option("graphics", {default = true, showmenu = true, description = "Build the rendering module"})
option("tests", {default = false, showmenu = true, description = "Build tests"})

includes("xmake/common.lua")

includes("log/*/xmake.lua")
includes("shared/*/xmake.lua")
includes("common/xmake.lua")
includes("engine/*/xmake.lua")
includes("network/*/xmake.lua")
includes("server/*/xmake.lua")
if has_config("graphics") then
    includes("client/*/xmake.lua")
    add_requires("raylib")
end
includes("tests/xmake.lua")
