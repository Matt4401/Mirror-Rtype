-- RLSS_PROJECT_DIR: the repository root, as a C string literal, for the RLSS tests (they move there to load the Wren
-- scripts). Written with forward slashes: on Windows, "$(projectdir)" has backslashes, which a C string literal reads
-- as escape sequences ("D:\a\..." starts with a bell character), so the path would be wrong.
rule("rlss.project_dir")
    on_load(function (target)
        local root = os.projectdir():gsub("\\", "/")
        target:add("defines", 'RLSS_PROJECT_DIR="' .. root .. '"')
    end)
rule_end()

target("test_collision_router")
    set_kind("binary")
    add_deps("rtype_collision")
    add_packages("gtest")
    add_files("server/CollisionRouterTests.cpp")
    add_tests("test_collision_router")

target("test_level_loader")
    set_kind("binary")
    add_deps("rtype_level")
    add_packages("gtest")
    add_files("server/LevelLoaderTests.cpp")
    -- The shipped levels are loaded from the source tree, whatever the directory the tests run from.
    -- path.unix: a Windows path would put backslashes, read as escape sequences, in the string literal.
    on_load(function (target)
        target:add("defines", 'RTYPE_LEVELS_DIR="' .. path.unix(path.join(os.projectdir(), "server", "levels")) .. '"')
    end)
    add_tests("test_level_loader")

target("test_prefabs")
    set_kind("binary")
    add_deps("engine_core", "engine_prefabs", "rtype_prefabs")
    add_packages("gtest")
    add_files("prefabs/PrefabManagerTests.cpp")
    add_tests("test_prefabs")

target("test_input")
    set_kind("binary")
    add_deps("engine_core", "engine_events", "engine_input")
    add_packages("gtest")
    add_files("test_input.cpp", "../client/src/input/RTypeBindings.cpp")
    add_includedirs("../client/src")
    add_tests("test_input")

target("test_server_room")
    set_kind("binary")
    add_deps("engine_core", "engine_time", "engine_events", "engine_network", "rtype_game", "rtype_gameplay")
    add_packages("gtest")
    add_files("server/RoomInputTests.cpp", "../server/src/Room.cpp")
    add_includedirs("../server/src")
    add_tests("test_server_room")

target("test_server_gameplay")
    set_kind("binary")
    add_deps("engine_core", "engine_time", "engine_events", "engine_physics", "engine_network", "rtype_gameplay")
    add_packages("gtest")
    add_files("server/GameplayTests.cpp")
    -- The shipped boss script is loaded from the source tree, whatever the directory the tests run from.
    -- path.unix: a Windows path would put backslashes, read as escape sequences, in the string literal.
    on_load(function (target)
        target:add("defines", 'RTYPE_SCRIPTS_DIR="' .. path.unix(path.join(os.projectdir(), "scripts")) .. '"')
    end)
    add_tests("test_server_gameplay")

target("test_server_parsing")
    set_kind("binary")
    add_deps("common")
    add_packages("gtest")
    add_files("server/ParsingTests.cpp", "../server/src/parsing/Parsing.cpp")
    add_includedirs("../server/src")
    add_tests("test_server_parsing")

target("test_protocol")
    set_kind("binary")
    add_deps("common")
    add_packages("gtest")
    add_files("test_protocol.cpp")
    add_tests("test_protocol")

target("test_server_manager")
    set_kind("binary")
    add_deps("engine_log", "common", "engine_core", "engine_time", "engine_events", "engine_network", "rtype_game", "rtype_gameplay")
    add_packages("gtest")
    add_files("server/ServerManagerTests.cpp", "../server/src/ServerManager.cpp", "../server/src/Lobby.cpp", "../server/src/Room.cpp")
    add_includedirs("../server/src")
    add_tests("test_server_manager")

target("test_client_collisions")
    set_kind("binary")
    add_deps("engine_core", "engine_events", "engine_physics", "engine_prefabs", "rtype_collision")
    add_packages("gtest")
    add_files("client/ShotHitRuleTests.cpp", "../client/src/gameplay/ShotHitRule.cpp",
              "../client/src/prefabs/ClientBullet.cpp", "../client/src/prefabs/ClientEnemy.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_collisions")

target("test_client_blink")
    set_kind("binary")
    add_deps("engine_core", "rtype_game")
    add_packages("gtest")
    add_files("client/ShipBlinkTests.cpp", "../client/src/gameplay/ShipBlink.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_blink")

target("test_client_autofire")
    set_kind("binary")
    add_packages("gtest")
    add_files("client/AutoFireTests.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_autofire")

target("test_rtype_game")
    set_kind("binary")
    add_deps("engine_core", "engine_time", "rtype_game")
    add_packages("gtest")
    add_files("test_rtype_game.cpp")
    add_tests("test_rtype_game")

target("test_server_e2e")
    set_kind("binary")
    add_deps("engine_log", "common", "engine_core", "engine_time", "engine_events", "engine_network", "engine_network_metrics", "engine_network_asio", "engine_input", "rtype_game", "rtype_gameplay")
    add_packages("gtest", "asio")
    add_files("server/EndToEndTests.cpp", "../server/src/ServerManager.cpp", "../server/src/Lobby.cpp", "../server/src/Room.cpp", "../client/src/net/RTypeNetworkPlugin.cpp", "../client/src/net/PlayerControlPlugin.cpp", "../client/src/metrics/ShipMetricsPlugin.cpp")
    add_includedirs("../server/src", "../client/src")
    add_tests("test_server_e2e")

target("test_client_network")
    set_kind("binary")
    add_deps("engine_log", "common", "engine_core", "engine_time", "engine_events", "engine_network", "engine_input", "rtype_game")
    add_packages("gtest")
    add_files("client/NetworkPluginTests.cpp", "../client/src/ClientOptions.cpp", "../client/src/net/RTypeNetworkPlugin.cpp", "../client/src/net/PlayerControlPlugin.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_network")

target("test_client_metrics")
    set_kind("binary")
    add_deps("engine_core", "engine_metrics", "engine_network")
    add_packages("gtest")
    add_files("client/ShipMetricsTests.cpp", "../client/src/metrics/ShipMetricsPlugin.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_metrics")

target("test_client_scenes")
    set_kind("binary")
    add_rules("rlss.project_dir")
    add_deps("engine_log", "common", "engine_core", "engine_time", "engine_events", "engine_network", "engine_input",
             "engine_hierarchy", "engine_ui", "engine_scene", "engine_scripting", "engine_scene_scripting",
             "engine_prefabs", "engine_config", "engine_settings", "engine_metrics", "engine_metrics_overlay",
             "rtype_game")
    add_packages("gtest", "wren")
    add_files("client/ClientScenesTests.cpp", "../client/src/net/RTypeNetworkPlugin.cpp",
              "../client/src/net/PlayerControlPlugin.cpp", "../client/src/scenes/*.cpp",
              "../client/src/input/RTypeBindings.cpp")
    add_includedirs("../client/src")
    add_tests("test_client_scenes")

target("test_rlss_protocol")
    set_kind("binary")
    add_deps("rlss_shared")
    add_packages("gtest")
    add_files("rlss/ProtocolTests.cpp")
    add_tests("test_rlss_protocol")

target("test_rlss_car")
    set_kind("binary")
    add_deps("rlss_game")
    add_packages("gtest", "wren")
    add_files("rlss/CarPhysicsTests.cpp")
    -- The Wren scripts are loaded relative to the working directory.
    add_rules("rlss.project_dir")
    add_tests("test_rlss_car")

target("test_rlss_codec")
    set_kind("binary")
    add_deps("rlss_game")
    add_packages("gtest")
    add_files("rlss/CodecTests.cpp")
    add_tests("test_rlss_codec")

target("test_rlss_menu")
    set_kind("binary")
    add_deps("rlss_shared", "rlss_game", "engine_log", "engine_core", "engine_time", "engine_events", "engine_network",
             "engine_input", "engine_hierarchy", "engine_ui", "engine_scene", "engine_config", "engine_settings")
    add_packages("gtest")
    add_files("rlss/MenuScenesTests.cpp", "../rlss/client/menu/*.cpp", "../rlss/client/ServerLink.cpp",
              "../rlss/client/Controls.cpp")
    add_includedirs("../rlss/client")
    add_tests("test_rlss_menu")

target("test_rlss_server")
    set_kind("binary")
    add_deps("rlss_game", "engine_log", "engine_core", "engine_time", "engine_events", "engine_network", "engine_scripting")
    add_packages("gtest", "wren")
    add_files("rlss/MatchServerTests.cpp", "../rlss/server/MatchServer.cpp", "../rlss/server/MatchRoom.cpp",
              "../rlss/server/RosterPlugin.cpp", "../rlss/server/CarSimulationPlugin.cpp")
    add_includedirs("../rlss/server")
    -- Each match runs the Wren scripts, loaded relative to the working directory.
    add_rules("rlss.project_dir")
    add_tests("test_rlss_server")

target("test_rlss_camera")
    set_kind("binary")
    add_deps("rlss_game")
    add_packages("gtest", "wren")
    add_files("rlss/CameraTests.cpp")
    add_rules("rlss.project_dir")
    add_tests("test_rlss_camera")
