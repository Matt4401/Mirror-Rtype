target("shared")
    set_kind("headeronly")
    add_headerfiles("**.hpp")
    add_includedirs(".", {public = true})
