set_project("R-type")
set_version("0.1.0")
set_languages("c++23")
add_rules("mode.debug", "mode.release")
add_rules("plugin.compile_commands.autoupdate", {outputdir = "."})

add_requires("asio", "gtest", "wren")

if is_plat("windows") then
    add_defines("_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

option("graphics", {default = true, showmenu = true, description = "Build the rendering module"})
option("tests", {default = false, showmenu = true, description = "Build tests"})

includes("xmake/common.lua")

includes("shared/*/xmake.lua")
includes("engine/*/xmake.lua")
includes("network/*/xmake.lua")
includes("server/*/xmake.lua")
if has_config("graphics") then
    includes("client/*/xmake.lua")
    add_requires("raylib")
end
includes("tests/xmake.lua")
