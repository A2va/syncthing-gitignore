add_rules("mode.debug", "mode.release")
set_languages("cxx17")

add_requires("tbox", {configs = {charset = true}})
add_requires("doctest")

target("gitignore_parser")
    set_kind("static")
    add_files("src/gitignore_parser.cpp")
    add_headerfiles("src/gitignore_parser.hpp")

target("utils")
    set_kind("static")
    add_files("src/utils.c")
    add_headerfiles("src/utils.h")
    add_packages("tbox")

target("synctignore")
    add_files("src/main.cpp")
    add_packages("tbox")
    add_deps("utils")

    -- used for win32 api (also compospolitan)
    add_defines("UNICODE", "_UNICODE")
    -- if is_plat("windows") then
    --     add_ldflags("/LTCG")
    -- end

target("tests")
    set_default(false)
    add_files("src/tests.cpp")
    add_deps("gitignore_parser")
    add_packages("doctest")