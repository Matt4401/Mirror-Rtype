target("test_ecs")
    set_kind("binary")
    add_deps("engine")
    add_packages("gtest")
    add_files("test_ecs.cpp")
    add_tests("test_ecs")

target("test_scripting")
    set_kind("binary")
    add_deps("engine")
    add_packages("gtest", "wren")
    add_files("test_scripting.cpp")
    add_tests("test_scripting")

target("test_clock")
    set_kind("binary")
    add_deps("engine")
    add_packages("gtest")
    add_files("test_clock.cpp")
    add_tests("test_clock")

if has_config("graphics") then
    target("test_render")
        set_kind("binary")
        add_deps("engine_render")
        add_packages("gtest")
        add_files("test_render.cpp")
        add_tests("test_render")
end
