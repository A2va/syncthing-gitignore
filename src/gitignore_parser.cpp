// MIT License

// Copyright (c) 2025 A2va

// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// C++ implementation of the python package gitignore_parser: https://github.com/mherrmann/gitignore_parser

#include <cstdint>
#include <fstream>
#include <stdexcept>

#include "gitignore_parser.hpp"
#include <iostream>

namespace fs = std::filesystem;

fs::path normalize_path(const fs::path& path) {
    fs::path normalized = fs::absolute(path).lexically_normal();

    // Strip trailing slash/backslash if the path isn't already the root directory
    if (!normalized.empty()) {

        if ((normalized != normalized.root_path()) && !normalized.has_filename()) {
            normalized = normalized.parent_path();
        }
    }

    // // Absolute remove the dot from an empty extension
    if (!path.empty() && path.extension() == ".") {
        fs::path filename = path.filename();

        filename += ".";
        normalized = normalized.parent_path() / filename;
    }

    return normalized;
}

void remove_trailing_characters(std::string &str, const char charToRemove) {
    str.erase(str.find_last_not_of(charToRemove) + 1, std::string::npos);
}

std::string process_substring(const std::string& pattern, size_t i, size_t j) {
    std::string stuff = pattern.substr(i, j); // Extract substring

    // Replace '\' with '\\'
    size_t pos = 0;
    while ((pos = stuff.find('\\', pos)) != std::string::npos) {
        stuff.replace(pos, 1, "\\\\");
        pos += 2; // Move past the newly inserted '\\'
    }

    // Remove all '/'
    stuff.erase(std::remove(stuff.begin(), stuff.end(), '/'), stuff.end());

    return stuff;
}

// Converts gitignore patterns to regex
std::string fnmatch_pathname_to_regex(
    const std::string pattern,
    bool directory_only,
    bool negation,
    bool anchored
) {
    char sep = fs::path::preferred_separator;
    std::string seps_group;
    if (sep == '\\') {
        seps_group = R"([\\/])";  // Handle both '/' and '\' on Windows
    }
    else {
        seps_group = std::string(1, sep);
    }
    std::string nonsep = "[^" + (sep == '\\' ? "\\\\" : std::string(1, sep)) + "/]";

    std::string regex_str;
    size_t i = 0, n = pattern.size();

    while (i < n) {
        char c = pattern[i++];
        if (c == '*') {
            try {
                if (i < n && pattern.at(i) == '*') {
                    i++;
                    if (i < n && pattern.at(i) == '/') {
                        i++;
                        regex_str += "(.*" + seps_group + ")?";
                    }
                    else {
                        regex_str += ".*";
                    }
                }
                else {
                    regex_str += nonsep + "*";
                }
            }
            catch (const std::out_of_range&) {
                regex_str += nonsep + "*";
            }
        }
        else if (c == '?') {
            regex_str += nonsep;
        }
        else if (c == '/') {
            regex_str += seps_group;
        }
        else if (c == '[') {
            size_t j = i;
            if (j < n && pattern[j] == '!') j++;
            if (j < n && pattern[j] == ']') j++;

            while (j < n && pattern[j] != ']') j++;
            if (j >= n) {
                regex_str += "\\[";
            }
            else {
                std::string cls = process_substring(pattern, i, j - i);
                i = j + 1;
                if (cls[0] == '!') {
                    cls = '^' + cls.substr(1);
                }
                else {
                    // cls = '\\' + cls;
                    cls = cls;
                }

                regex_str += "[" + cls + "]";
            }
        }
        else {
            if (std::string("^$.|?*+()[]{}").find(c) != std::string::npos) {
                regex_str += '\\';
            }
            regex_str += c;
        }
    }

    if (anchored) regex_str = "^" + regex_str;
    else regex_str = "(^|" + seps_group + ")" + regex_str;

    if (directory_only) {
        if (!negation) regex_str += "($|" + seps_group + ")";
        else regex_str += "/$";
    }
    else {
        regex_str += "$";
    }

    return regex_str;
}

bool IgnoreRule::match(const fs::path& abs_path) const {
    try {
        fs::path rel_path = normalize_path(abs_path);
        if(base_path) {
            fs::path directory = rel_path.parent_path();
            fs::path base_p = base_path.value();
            if (std::mismatch(base_p.begin(), base_p.end(), directory.begin()).first != base_p.end()) {
                
                // throw std::runtime_error("Given path is not in the subpath of the base path");
            }
            rel_path = std::filesystem::relative(normalize_path(abs_path), base_path.value());
        }

        std::string rel_str = rel_path.string();

        if (rel_str.empty()) rel_str = ".";
        if (rel_str.substr(0, 2) == "./") rel_str.erase(0, 2);

        if (directory_only && negation && abs_path.has_filename())
            rel_str += '/';

        return std::regex_search(rel_str, regex);
    } catch (const std::filesystem::filesystem_error&) {
        return false;
    }
}

std::optional<IgnoreRule> rule_from_pattern(const std::string& orig_pattern, const std::optional<fs::path>& base_path, const std::optional<std::pair<fs::path, int>>& source) {
    std::string pattern = orig_pattern;

    // Ignore comments and empty lines
    if (pattern.empty() || pattern[0] == '#') return std::nullopt;

    // Strip leading bang before examining double asterisks
    bool negation = false;
    if (pattern[0] == '!') {
        negation = true;
        pattern.erase(0, 1);
    }

    // Multi-asterisks not surrounded by slashes (or at the start/end) should
    // be treated like single-asterisks.
    pattern = std::regex_replace(pattern, std::regex(R"(([^/])\*{2,})"), "$1*");
    pattern = std::regex_replace(pattern, std::regex(R"(\*{2,}([^/]))"), "*$1");
 
    // Special-casing '/', which doesn't match any files or directories
    remove_trailing_characters(pattern, ' ');
    if (pattern == "/")
        return std::nullopt;

    bool directory_only = pattern[pattern.size() - 1] == '/';

    // A slash is a sign that we're tied to the base_path of our rule
    // set.
    bool anchored = pattern.substr(0, pattern.size() - 1).find('/') != std::string::npos;

    if (pattern[0] == '/') 
        pattern = pattern.substr(1);
    if ((pattern[0] == '*') && (pattern.size() >= 2) && (pattern[1] == '*')) {
        pattern = pattern.substr(2);
        anchored = false;
    }
    if (pattern[0] == '/')
        pattern = pattern.substr(1);
    if (pattern.back() == '/')
        pattern = pattern.substr(0, pattern.size() - 1);

    // patterns with leading hashes or exclamation marks are escaped with a
    // backslash in front, unescape it
    if((pattern[0] == '\\') && (pattern[1] == '#' || pattern[1] == '!')) {
        pattern = pattern.substr(1);
    }

    uint32_t i = pattern.size() - 1;
    bool strip_trailing_slash = false;

    // trailing spaces are ignored unless they are escaped with a backslash
    while (i > 1 && pattern[i] == ' ') {
        if (pattern[i - 1] == '\\') {
            pattern = pattern.substr(0, i - 1) + pattern.substr(i);
            i--;
            strip_trailing_slash = false;
        } else {
            if (strip_trailing_slash) {
                pattern = pattern.substr(0, i);
            }
            i--;
        }
    }

    std::string regex_str = fnmatch_pathname_to_regex(pattern, directory_only, negation, anchored);

    return IgnoreRule(orig_pattern, regex_str, negation, directory_only, anchored, base_path, source);
}

// Parses a .gitignore file into rules
std::vector<IgnoreRule> parse_gitignore(const fs::path& path, std::optional<fs::path> base_dir = std::nullopt) {
    if (!base_dir) {
        base_dir = std::optional(path.parent_path());
    }

    if (base_dir) {
        base_dir = std::optional(normalize_path(base_dir.value()));
    }
    // fs::path base = base_dir.empty() ? path.parent_path() : base_dir;
    std::vector<IgnoreRule> rules;
        
    if (!std::filesystem::exists(path)) {
        return rules;
    }

    std::ifstream file(path);
    std::string line;
    int line_num = 0;

    while (std::getline(file, line)) {
        line_num++;
        remove_trailing_characters(line, '\n');

        std::optional<IgnoreRule> rule = rule_from_pattern(line, base_dir, std::optional(std::make_pair(path, line_num)));
        if(rule) {
            rules.push_back(*rule);
        }
    }

    return rules;
}