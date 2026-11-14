-- Package mode (--engine_package=y): the engine comes from the installed `squacaron` package, not from engine/.
-- Each engine target name becomes a phony target that forwards the package, so the game targets keep their
-- `add_deps("engine_core", ...)` lines in both modes.
local engine_targets = {
    "engine_all", "engine_assets", "engine_assets_audio", "engine_assets_render", "engine_assets_scene",
    "engine_assets_scripting", "engine_audio", "engine_config", "engine_core", "engine_devconsole", "engine_events",
    "engine_headers", "engine_hierarchy", "engine_input", "engine_log", "engine_metrics", "engine_metrics_overlay", "engine_network",
    "engine_network_asio", "engine_network_metrics", "engine_physics", "engine_prefabs", "engine_render",
    "engine_scene", "engine_scene_scripting", "engine_scripting", "engine_settings", "engine_time",
    "engine_timeline", "engine_tools", "engine_tools_scripting", "engine_ui"
}

for _, name in ipairs(engine_targets) do
    target(name)
        set_kind("phony")
        add_packages("squacaron", {public = true})
    target_end()
end
