#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <fstream>
#include <filesystem>
#include <random>
#include <vector>
#include <string>

#include <doctest/doctest.h>
#include "gitignore_parser.hpp"

// Tests coming from the python package gitignore_parser: https://github.com/mherrmann/gitignore_parser

namespace fs = std::filesystem;

class TemporaryDirectory {
    public:
        TemporaryDirectory() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 15);
            const char* hex = "0123456789abcdef";
            do {
                std::string suffix;
                for (int i = 0; i < 16; ++i) {
                    suffix += hex[dis(gen)];
                }
                path = fs::temp_directory_path() / ("test_" + suffix);
            } while (fs::exists(path));
            fs::create_directories(path);
        }
    
        ~TemporaryDirectory() {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    
        fs::path get_path() const { return path; }
    
    private:
        fs::path path;
};

TEST_CASE("simple") {
   TemporaryDirectory temp_dir;
   fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
   {
       std::ofstream file(gitignore_path);
       file << "__pycache__/\n";
       file << "*.py[cod]";
   }
   GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

   CHECK_FALSE(matcher.is_ignored("/home/a2va/main.py"));
   CHECK(matcher.is_ignored("/home/a2va/main.pyc"));
   CHECK(matcher.is_ignored("/home/a2va/dir/main.pyc"));
   CHECK(matcher.is_ignored("/home/a2va/__pycache__"));
}

TEST_CASE("incomplete filename") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "o.py";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/o.py"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/foo.py"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/foo.pyc"));
    CHECK(matcher.is_ignored("/home/a2va/dir/o.py"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/dir/foo.py"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/dir/foo.pyc"));
}

TEST_CASE("wildcard") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "hello.*\n";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/hello.txt"));
    CHECK(matcher.is_ignored("/home/a2va/hello.foobar/"));
    CHECK(matcher.is_ignored("/home/a2va/dir/hello.txt"));
    CHECK(matcher.is_ignored("/home/a2va/hello."));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/dir/hello"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/dir/helloX"));
}

TEST_CASE("anchored wildcard") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "/hello.*\n";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/hello.txt"));
    CHECK(matcher.is_ignored("/home/a2va/hello.c/"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/a/hello.java"));
}

// These tests do not pass for now because of the normalize path that strips out the whitespaces
// TEST_CASE("trailing whitespaces") {
//     TemporaryDirectory temp_dir;
//     fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
//     {
//         std::ofstream file(gitignore_path);
//         file << "ignoretrailingspace \n";
//         file << "notignoredspace\\ \n";
//         file << "partiallyignoredspace\\  \n";
//         file << "partiallyignoredspace2 \\  \n";
//         file << "notignoredmultiplespace\\ \\ \\ ";
//     }

//     GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");
//     // self.assertTrue(matches('/home/a2va/ignoretrailingspace'))
//     CHECK(matcher.is_ignored("/home/a2va/ignoretrailingspace"));

//     // self.assertFalse(matches('/home/a2va/ignoretrailingspace '))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/ignoretrailingspace "));

//     //  self.assertTrue(matches('/home/a2va/partiallyignoredspace '))
//     CHECK(matcher.is_ignored("/home/a2va/partiallyignoredspace "));
//     // self.assertFalse(matches('/home/a2va/partiallyignoredspace  '))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/partiallyignoredspace  "));
//     // self.assertFalse(matches('/home/a2va/partiallyignoredspace'))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/partiallyignoredspace"));

//     // self.assertTrue(matches('/home/a2va/partiallyignoredspace2  '))
//     CHECK(matcher.is_ignored("/home/a2va/partiallyignoredspace2  "));
//     // self.assertFalse(matches('/home/a2va/partiallyignoredspace2   '))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/partiallyignoredspace2   "));
//     //  self.assertFalse(matches('/home/a2va/partiallyignoredspace2 '))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/partiallyignoredspace2 "));
//     //  self.assertFalse(matches('/home/a2va/partiallyignoredspace2'))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/partiallyignoredspace2"));

//     // self.assertTrue(matches('/home/a2va/notignoredspace '))
//     CHECK(matcher.is_ignored("/home/a2va/notignoredspace "));
//     // self.assertFalse(matches('/home/a2va/notignoredspace'))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/notignoredspace"));

//     // self.assertTrue(matches('/home/a2va/notignoredmultiplespace   '))
//     CHECK(matcher.is_ignored("/home/a2va/notignoredmultiplespace   "));
//     // self.assertFalse(matches('/home/a2va/notignoredmultiplespace'))
//     CHECK_FALSE(matcher.is_ignored("/home/a2va/notignoredmultiplespace"));
// }

TEST_CASE("comment") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "somematch\n";
        file << "#realcomment\n"; 
        file << "othermatch\n";
        file << "\\#imnocomment";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/somematch"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/#realcomment"));
    CHECK(matcher.is_ignored("/home/a2va/othermatch"));
    CHECK(matcher.is_ignored("/home/a2va/#imnocomment"));

}

TEST_CASE("ignore directory") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << ".venv/\n";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/.venv"));   
    CHECK(matcher.is_ignored("/home/a2va/.venv/folder"));
    CHECK(matcher.is_ignored("/home/a2va/.venv/file.txt"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/.venv_other_folder"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/.venv_no_folder.py"));
}

TEST_CASE("ignore directory asterisk") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << ".venv/*";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK_FALSE(matcher.is_ignored("/home/a2va/.venv"));
    CHECK(matcher.is_ignored("/home/a2va/.venv/folder"));
    CHECK(matcher.is_ignored("/home/a2va/.venv/file.txt"));
}

TEST_CASE("negation") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "*.ignore\n";
        file << "!keep.ignore";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/trash.ignore"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/keep.ignore"));
    CHECK(matcher.is_ignored("/home/a2va/waste.ignore"));
}

TEST_CASE("literal exclamation mark") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "\\!ignore_me!";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK(matcher.is_ignored("/home/a2va/!ignore_me!"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/ignore_me!"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/ignore_me!"));
}


TEST_SUITE("asterisk") {

    TEST_CASE("double asterisks") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "foo/**/Bar";
        }
        GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");
    
        CHECK(matcher.is_ignored("/home/a2va/foo/hello/Bar"));
        CHECK(matcher.is_ignored("/home/a2va/foo/world/Bar"));
        CHECK(matcher.is_ignored("/home/a2va/foo/Bar"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/foo/BarBar"));
    }

    TEST_CASE("without slashes handled like single asterisk") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "a/b**c/d";
        }
        GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");
    
        CHECK(matcher.is_ignored("/home/a2va/a/bc/d"));
        CHECK(matcher.is_ignored("/home/a2va/a/bXc/d"));
        CHECK(matcher.is_ignored("/home/a2va/a/bbc/d"));
        CHECK(matcher.is_ignored("/home/a2va/a/bcc/d"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/a/bcd"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/a/b/c/d"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/a/bb/cc/d'"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/a/bb/XX/cc/d"));
    }

    TEST_CASE("handled like single asterisk 1") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "***a/b";
        }
        GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

        CHECK(matcher.is_ignored("/home/a2va/XYZa/b"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/foo/a/b"));
    }

    TEST_CASE("handled like single asterisk 2") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "a/b***";
        }
        GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

        CHECK(matcher.is_ignored("/home/a2va/a/bXYZ"));
        CHECK_FALSE(matcher.is_ignored("/home/a2va/a/b/foo"));
    }

    TEST_CASE("single asterisk") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "*";
        }
        GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

        CHECK(matcher.is_ignored("/home/a2va/file.txt"));
        CHECK(matcher.is_ignored("/home/a2va/directory"));
        CHECK(matcher.is_ignored("/home/a2va/directory-trailing/"));
    }
}


TEST_CASE("slash in range does not match dirs") {
    TemporaryDirectory temp_dir;
    fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
    {
        std::ofstream file(gitignore_path);
        file << "abc[X-Z/]def";
    }
    GitIgnoreMatcher matcher(gitignore_path, "/home/a2va");

    CHECK_FALSE(matcher.is_ignored("/home/a2va/abcdef"));
    CHECK(matcher.is_ignored("/home/a2va/abcXdef"));
    CHECK(matcher.is_ignored("/home/a2va/abcYdef"));
    CHECK(matcher.is_ignored("/home/a2va/abcZdef"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/abc/def"));
    CHECK_FALSE(matcher.is_ignored("/home/a2va/abcXYZdef"));
}

TEST_SUITE("windows style path") {

    TEST_CASE("simple") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "__pycache__/\n";
            file << "*.py[cod]";
        }
        GitIgnoreMatcher matcher(gitignore_path, "D:/home/a2va");

        CHECK_FALSE(matcher.is_ignored("D:/home/a2va/main.py"));
        CHECK(matcher.is_ignored("D:/home/a2va/main.pyc"));
        CHECK(matcher.is_ignored("D:/home/a2va/dir/main.pyc"));
        CHECK(matcher.is_ignored("D:/home/a2va/__pycache__"));
    }

    TEST_CASE("simple 2") {
        TemporaryDirectory temp_dir;
        fs::path gitignore_path = temp_dir.get_path() / ".gitignore";
        {
            std::ofstream file(gitignore_path);
            file << "__pycache__/\n";
            file << "*.py[cod]";
        }
        GitIgnoreMatcher matcher(gitignore_path, "D:/home/a2va");
 
        CHECK_FALSE(matcher.is_ignored("D:\\home\\a2va\\main.py"));
        CHECK(matcher.is_ignored("D:\\home\\a2va\\main.pyc"));
        CHECK(matcher.is_ignored("D:\\home\\a2va\\dir\\main.pyc"));
        CHECK(matcher.is_ignored("D:\\home\\a2va\\__pycache__"));
    }
}