set_project("R-type")
set_version("0.1.0")
set_languages("c++23")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("asio", "raylib", "gtest", "wren")

if is_plat("windows") then
    add_defines("_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

option("tests", {default = false, showmenu = true, description = "Build tests"})

includes("xmake/common.lua")

includes("network/*/xmake.lua")
includes("server/*/xmake.lua")
includes("client/*/xmake.lua")
target("engine")
    set_kind("static")
    add_files("engine/src/**.cpp", {optional = true})
    add_includedirs("engine/src", {public = true})
    add_packages("wren", {public = true})

target("r-type_server")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("server/**.cpp")
    add_deps("engine")
    add_packages("asio")
    add_tests("starts")

target("r-type_client")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("client/**.cpp")
    add_deps("engine")
    add_packages("asio", "raylib")
    add_tests("starts")

target("test_ecs")
    set_kind("binary")
    add_deps("engine")
    add_packages("gtest")
    add_files("tests/test_ecs.cpp")

target("test_scripting")
    set_kind("binary")
    add_deps("engine")
    add_packages("gtest", "wren")
    add_files("tests/test_scripting.cpp")
