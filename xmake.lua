add_rules("mode.debug", "mode.release")
set_languages("cxx17")

-- package("tbox-patched")
--     set_base("tbox")
--     on_test(function (package) 
--     end)
-- package_end()

-- add_requires("tbox-patched", {system = false, alias = "tbox"})
-- add_requires("tbox")
includes("tbox")

add_requires("doctest")

target("gitignore_parser")
    set_kind("static")
    add_files("src/gitignore_parser.cpp")
    add_headerfiles("src/gitignore_parser.hpp")

target("synctignore")
    add_files("src/main.c")
    -- add_packages("tbox")
    add_deps("tbox")

    -- used for win32 api (also compospolitan)
    add_defines("UNICODE", "_UNICODE")

target("tests")
    set_default(false)
    add_files("src/tests.cpp")
    add_deps("gitignore_parser")
    add_packages("doctest")