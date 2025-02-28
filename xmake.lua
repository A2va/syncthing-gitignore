add_rules("mode.debug", "mode.release")
set_languages("cxx17")

add_requires("tbox")
add_requires("doctest")

target("gitignore_parser")
    set_kind("static")
    add_files("src/gitignore_parser.cpp")
    add_headerfiles("src/gitignore_parser.hpp")

target("synctignore")
    add_files("src/main.cpp")
    add_packages("tbox")

    -- used for win32 api (also compospolitan)
    add_defines("UNICODE", "_UNICODE")

target("tests")
    add_files("src/tests.cpp")
    add_deps("gitignore_parser")
    add_packages("doctest")