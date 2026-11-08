-- Squacaron, the engine. Two ways to build it:
--   * alone: `cd engine && xmake` (this file is then the project root and declares the project, options and packages);
--   * inside a game repository that does `includes("engine/xmake.lua")`: the game's root file owns the project,
--     the options and the packages, and this file only adds the engine targets.
-- Games can also consume the installed engine as the `squacaron` xmake package (see xmake-repo/).
local standalone = path.absolute(os.projectdir()) == path.absolute(os.scriptdir())

if standalone then
    set_project("squacaron")
    set_version("0.2.0")
    set_languages("c++23")
    add_rules("mode.debug", "mode.release")

    option("graphics", {default = true, showmenu = true, description = "Build the render and audio modules (raylib)"})
    option("tests", {default = false, showmenu = true, description = "Build the engine tests"})

    add_requires("asio", "wren", "box2d v2.4.2", "toml++ v3.4.0")
    if has_config("graphics") then
        add_requires("raylib")
    end
    if has_config("tests") then
        add_requires("gtest")
    end

    if is_plat("windows") then
        add_defines("_WIN32_WINNT=0x0A00", "WIN32_LEAN_AND_MEAN", "NOMINMAX")
    end
end

includes("src/xmake.lua")

if not standalone or has_config("tests") then
    includes("tests/xmake.lua")
end
