target("network")
    set_kind("static")
    -- add_files("**.cpp")
    -- add_deps("shared")
    add_packages("asio")
    -- add_tests("starts")
    add_includedirs("./**.cpp")
    if is_plat("windows", "mingw") then
        add_syslinks("ws2_32", "mswsock")
    end