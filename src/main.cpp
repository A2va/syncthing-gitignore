#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "tbox/tbox.h"
#include "utils.hpp"
#include "gitignore_parser.hpp"

namespace fs = std::filesystem;

tb_int_t main(tb_int_t argc, tb_char_t** argv) {
    // Get current directory
    auto current_dir = fs::path(get_program_file()).parent_path();

    // Collect all .gitignore files and build ignore rules
    std::vector<GitIgnoreMatcher> matchers;
    
    // Walk directory tree to find .gitignore files
    auto find_gitignores = [](const fs::path& path, const fs::directory_entry& entry, 
                             const std::vector<GitIgnoreMatcher>& matchers, std::vector<GitIgnoreMatcher>& output) {
        if (entry.path().filename() == ".gitignore") {
            output.push_back(GitIgnoreMatcher(entry.path(), entry.path().parent_path()));
        }
    };

    std::vector<GitIgnoreMatcher> gitignore_matchers;
    fs::recursive_directory_iterator dir_iter(current_dir);
    for (const auto& entry : dir_iter) {
        std::cout << entry.path() << std::endl;
        if (entry.path().filename() == ".gitignore") {
            gitignore_matchers.emplace_back(entry.path(), entry.path().parent_path());
        }
    }
    return 0;
}
