target("r-type_client")
    set_kind("binary")
    set_targetdir("$(projectdir)")
    add_files("**.cpp")
    add_includedirs(".")
    add_deps("engine_core", "engine_time", "engine_events", "engine_scripting", "engine_render", "engine_input", "engine_physics", "engine_audio", "engine_config", "engine_prefabs", "engine_hierarchy", "engine_ui", "engine_metrics", "engine_metrics_overlay", "engine_network", "engine_network_metrics", "engine_network_asio", "rtype_game", "rtype_collision", "engine_scene", "engine_scene_scripting", "engine_settings", "engine_tools", "engine_devconsole", "engine_tools_scripting")
    add_packages("asio", "wren")
    if is_plat("linux") then
        -- A libxcb built by xmake makes raylib link all its modules, used or not, and the system libxcb may lack some
        -- (libxcb-xevie, libxcb-ge), which stops the binary from loading: only keep the ones actually needed.
        -- The list comes from the packages because a system libx11 provides no module, and asking the linker for a
        -- missing library is an error even with --as-needed. Not add_ldflags: xmake places it after the libraries,
        -- where --as-needed has no effect.
        on_config(function (target)
            local modules = {}
            for _, links in ipairs(table.wrap(target:get_from("links", "*"))) do
                for _, link in ipairs(table.wrap(links)) do
                    if link:startswith("xcb-") then
                        table.insert(modules, link)
                    end
                end
            end
            if #modules > 0 then
                target:add("linkgroups", table.unique(modules), {as_needed = true})
            end
        end)
    end
