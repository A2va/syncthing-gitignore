add_rules("mode.debug", "mode.release")
set_languages("cxx20")

package("tbox-patched")
    set_base("tbox")
    -- add_patches("v1.7.6", "https://github.com/tboox/tbox/commit/bf717ac71bc6b7c95695d6a2c9b0c8dc11331cb9.patch", "530e78af853b5b8d80a71dbe9463a604bee55e6107415d74b53ed7a7bfd59121")
    on_load(function (package)
        if package:has_tool("cc", "cosmocc") then
            package:add("patches", "v1.7.6", "https://github.com/tboox/tbox/commit/bf717ac71bc6b7c95695d6a2c9b0c8dc11331cb9.patch", "530e78af853b5b8d80a71dbe9463a604bee55e6107415d74b53ed7a7bfd59121")
        end
    end)

package_end()

-- enable micro tbox because cosmocc patch will become bigger without it
add_requires("tbox", {configs = {charset = true, micro = true}})
add_requires("doctest", "nlohmann_json")

target("gitignore_parser")
    set_kind("static")
    add_files("src/gitignore_parser.cpp")
    add_deps("utils")
    add_headerfiles("src/gitignore_parser.hpp")

target("utils")
    set_kind("static")
    add_files("src/cosmocc.c", "src/utils.cpp")
    add_headerfiles("src/cosmocc.h")
    add_packages("tbox", {public = true})

target("synctignore")
    set_rundir("$(projectdir)")
    add_files("src/main.cpp")
    add_deps("utils", "gitignore_parser")
    add_packages("nlohmann_json")

    -- used for win32 api (also compospolitan)
    add_defines("UNICODE", "_UNICODE")

target("tests")
    set_default(false)
    add_files("src/tests.cpp")
    add_deps("gitignore_parser", "utils")
    add_packages("doctest")