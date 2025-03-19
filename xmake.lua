add_rules("mode.debug", "mode.release")
set_languages("cxx20")

add_requires("tbox", {configs = {charset = true}})
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