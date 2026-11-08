-- The Squacaron engine as an xmake package, built from the engine/ directory of this repository.
-- A game outside this repository would point `set_sourcedir` (or `add_urls`) at its own copy of the engine.
-- xmake does not rebuild a local package when its sources change: run `xmake require --force squacaron` after an
-- engine change, or build the games in-tree (the default, without --engine_package).
package("squacaron")
    set_description("Squacaron: a modular, data-oriented C++23 game engine")
    set_sourcedir(path.join(os.scriptdir(), "..", "..", "..", "..", "engine"))

    add_configs("graphics", {description = "Build the render and audio modules (raylib).", default = true, type = "boolean"})

    add_deps("asio", "wren", "box2d v2.4.2", "toml++ v3.4.0")

    on_load(function (package)
        if package:config("graphics") then
            package:add("deps", "raylib")
        end
        package:add("includedirs", "include/squacaron")
        -- Static libraries: a library comes before the ones it depends on.
        local links = {"engine_scene_scripting", "engine_scene", "engine_metrics_overlay", "engine_network_metrics",
                       "engine_network_asio", "engine_network", "engine_physics", "engine_scripting",
                       "engine_prefabs", "engine_input", "engine_settings"}
        if package:config("graphics") then
            table.join2(links, {"engine_audio"})
        end
        table.join2(links, {"engine_config", "engine_timeline"})
        if package:config("graphics") then
            table.join2(links, {"engine_render"})
        end
        table.join2(links, {"engine_ui", "engine_metrics", "engine_hierarchy", "engine_log", "engine_core", "engine_time"})
        package:add("links", links)
        if package:is_plat("windows", "mingw") then
            package:add("syslinks", "winmm", "psapi", "user32", "ws2_32", "mswsock")
        end
    end)

    on_install(function (package)
        import("package.tools.xmake").install(package, {graphics = package:config("graphics"), tests = false})
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include "core/Engine.hpp"
            void test() { engine::Engine engine; }
        ]]}, {configs = {languages = "c++23"}}))
    end)
