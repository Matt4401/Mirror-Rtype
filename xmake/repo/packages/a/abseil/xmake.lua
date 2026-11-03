-- Local override of the xmake-repo "abseil" recipe, trimmed to the version protobuf-cpp 31.1 resolves to.
-- It only adds the upstream fix for abseil/abseil-cpp#1960: with MSVC in C++20, flat_hash_map<K, std::variant<...>>
-- whose variant holds an incomplete type fails to compile, which breaks protobuf's json/internal/untyped_message.h.
-- Drop this repository once xmake-repo ships abseil >= 20260817.0 and protobuf-cpp accepts it.
package("abseil")
    set_homepage("https://abseil.io")
    set_description("C++ Common Libraries")
    set_license("Apache-2.0")

    add_urls("https://github.com/abseil/abseil-cpp/archive/refs/tags/$(version).tar.gz",
             "https://github.com/abseil/abseil-cpp.git")

    add_versions("20260107.1", "4314e2a7cbac89cac25a2f2322870f343d81579756ceff7f431803c2c9090195")

    -- https://github.com/abseil/abseil-cpp/commit/14d4aed2dd4ed7c1bc3025ef4a4a50b0aa4af06f (absl/meta/type_traits.h only)
    add_patches("20260107.1", path.join(os.scriptdir(), "patches", "20260107.1", "variant-lifetimebound.patch"),
                "62b3bd50446ce29d44a002d2d9381682c1e39fa6e66eeac945238e8c88afb58a")

    add_deps("cmake")

    add_configs("cxx_standard", {description = "Select c++ standard to build.", default = "17", type = "string", values = {"14", "17", "20"}})
    -- xmake leaves patches out of the build hash: this config gives the patched build its own install directory,
    -- so an unpatched abseil already installed on a machine is not reused.
    add_configs("variant_lifetimebound_fix", {description = "Apply the abseil#1960 patch.", default = true, type = "boolean", readonly = true})

    on_load(function (package)
        if package:is_plat("windows", "mingw", "msys") then
            package:add("syslinks", "advapi32", "dbghelp", "bcrypt")
        elseif package:is_plat("linux", "bsd") then
            package:add("syslinks", "pthread")
        elseif package:is_plat("macosx", "iphoneos") then
            package:add("frameworks", "CoreFoundation")
        end

        if package:is_plat("windows") and package:config("shared") then
            package:add("defines", "ABSL_CONSUME_DLL")
        end
    end)

    on_install(function (package)
        io.replace("CMakeLists.txt", [[set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")]], "", {plain = true})
        io.replace("CMakeLists.txt", [[set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")]], "", {plain = true})

        local configs = {
            "-DCMAKE_CXX_STANDARD=" .. package:config("cxx_standard"),
            "-DABSL_ENABLE_INSTALL=ON",
            "-DABSL_PROPAGATE_CXX_STD=ON",
        }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs, {builddir = os.tmpfile() .. ".dir"})

        -- get links and ensure link order
        import("core.base.graph")
        local dag = graph.new(true)
        local pkgconfigdir = package:installdir("lib", "pkgconfig")
        for _, pcfile in ipairs(os.files(path.join(pkgconfigdir, "*.pc"))) do
            local link = path.basename(pcfile)
            local content = io.readfile(pcfile)
            for _, line in ipairs(content:split("\n")) do
                if line:startswith("Requires: ") then
                    local requires = line:sub(10):split(",")
                    for _, dep in ipairs(requires) do
                        dep = dep:split("=")[1]:trim()
                        -- absl_strings has self references
                        if dep ~= link then
                            dag:add_edge(link, dep)
                        end
                    end
                end
            end
        end
        local links = dag:topological_sort()
        package:add("links", links)

        local cycle = dag:find_cycle()
        if cycle then
            wprint("cycle links found", cycle)
        end
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            #include "absl/strings/str_join.h"
            #include <string>
            #include <vector>
            void test() {
                std::vector<std::string> v = {"foo", "bar", "baz"};
                std::string s = absl::StrJoin(v, "-");
            }
        ]]}, {configs = {languages = "cxx" .. package:config("cxx_standard")}}))
    end)
