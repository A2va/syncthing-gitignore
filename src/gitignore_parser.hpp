#ifndef GITIGNORE_PARSER_H
#define GITIGNORE_PARSER_H

#include <algorithm>
#include <utility>
#include <string>
#include <regex>
#include <vector>
#include <optional>
#include <filesystem>

struct IgnoreRule {
    std::string pattern;
    std::regex regex;
    bool negation;
    bool directory_only;
    bool anchored;
    std::optional<std::filesystem::path> base_path;
    std::optional<std::pair<std::filesystem::path, int>> source;

    IgnoreRule(
        const std::string& p, 
        const std::string& r, 
        bool neg, 
        bool dir_only,
        bool anch, 
        const std::optional<std::filesystem::path>& base,
        const std::optional<std::pair<std::filesystem::path, int>>& src
    ) : pattern(p), regex(r), negation(neg), directory_only(dir_only), 
        anchored(anch), base_path(base), source(src) {}

    bool match(const std::filesystem::path& abs_path) const;
};

std::vector<IgnoreRule> parse_gitignore(const std::filesystem::path& path, std::optional<std::filesystem::path> base_dir);


// Checks if a path is ignored based on rules
class GitIgnoreMatcher {
    std::vector<IgnoreRule> rules;
    bool has_negations;

public:
    GitIgnoreMatcher(const std::filesystem::path& gitignore_path, std::optional<std::filesystem::path> base_dir = std::nullopt) 
        : rules(parse_gitignore(gitignore_path, base_dir)),
          has_negations(std::any_of(rules.begin(), rules.end(), 
              [](const auto& r) { return r.negation; })) {}

    bool is_ignored(const std::filesystem::path& path) {
        if (has_negations) {
            for (auto it = rules.rbegin(); it != rules.rend(); ++it) {
                if (it->match(path)) return !it->negation;
            }
            return false;
        } else {
            for (const auto& rule : rules) {
                if (rule.match(path)) return true;
            }
            return false;
        }
    }
};

#endif