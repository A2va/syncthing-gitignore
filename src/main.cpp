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

    // Walk directory tree and collect ignored paths
    auto collect_ignored_paths = [](const fs::path& path, const fs::directory_entry& entry, 
                                  const std::vector<GitIgnoreMatcher>& matchers, std::vector<fs::path>& ignored_paths) {
        for (const auto& matcher : matchers) {
            if (matcher.is_ignored(entry.path())) {
                ignored_paths.push_back(entry.path());
                return TB_DIRECTORY_WALK_CODE_CONTINUE;
            }
        }
        return TB_DIRECTORY_WALK_CODE_CONTINUE;
    };

    std::vector<fs::path> ignored_paths;
    fs::recursive_directory_iterator dir_iter2(current_dir);
    for (const auto& entry : dir_iter2) {
        for (const auto& matcher : gitignore_matchers) {
            if (matcher.is_ignored(entry.path())) {
                ignored_paths.push_back(entry.path());
                break;
            }
        }
    }

    // Generate Syncthing ignore patterns
    std::cout << "# Syncthing ignore patterns generated from .gitignore files\n";
    for (const auto& path : ignored_paths) {
        fs::path relative_path = fs::relative(path, current_dir);
        
        // Generate patterns similar to the Python script
        std::string line = relative_path.string();
        if (line[0] == '/') {
            line = line.substr(1);
        }
        
        if (line.find('/') != std::string::npos) {
            std::cout << line << "\n";
            std::cout << "**/" << line << "\n";
        } else {
            std::cout << line << "\n";
            std::cout << "**/" << line << "\n";
        }
    }

    return 0;
}
