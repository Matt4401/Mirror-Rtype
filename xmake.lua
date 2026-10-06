set_project("R-type")
set_version("0.1.0")
set_languages("c++23")
add_rules("mode.debug", "mode.release")

add_requires("asio", "raylib")

add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

if is_plat("windows") then
    add_defines("_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

option("tests", {default = false, showmenu = true, description = "Build tests"})

includes("xmake/common.lua")

includes("shared/*/xmake.lua")
includes("network/*/xmake.lua")
includes("server/*/xmake.lua")
includes("client/*/xmake.lua")
--[[ target("engine")
    set_kind("static")
    add_files("engine/**.cpp")
    add_includedirs("engine", {public = true})]]--
