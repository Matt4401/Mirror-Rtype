target("engine")
    set_kind("static")
    add_files("**.cpp|render/**.cpp|systems/render/**.cpp")
    add_includedirs(".", {public = true})
    add_packages("wren", "box2d", {public = true})

if has_config("graphics") then
    includes("render")
end
