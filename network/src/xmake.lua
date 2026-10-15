target("network")
    set_kind("static")
    add_files("**.cpp")
    add_deps("shared")
    add_includedirs(".", {public = true})
    add_packages("asio")
    -- add_tests("starts")

    if is_plat("windows", "mingw") then
        add_syslinks("ws2_32", "mswsock")
    end
