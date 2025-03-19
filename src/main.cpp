#include <filesystem>
#include <optional>
#include <iostream>
#include <fstream>
#include <string>
#include <set>

#include "gitignore_parser.hpp"
#include "tbox/tbox.h"
#include "utils.hpp"

namespace fs = std::filesystem;

std::vector<fs::path> collect_gitignore_files(const fs::path& path)
{
	std::vector<fs::path> gitignore_files;
	fs::recursive_directory_iterator dir_iter(normalize_path(path));
	for (const auto& entry : dir_iter)
	{
		std::cout << entry.path() << std::endl;
		if (entry.path().filename() == ".gitignore")
		{
			gitignore_files.emplace_back(entry.path());
		}
	}
	return gitignore_files;
}

void strip(std::string& str)
{
	if (str.length() == 0)
	{
		return;
	}

	auto start_it = str.begin();
	auto end_it = str.rbegin();
	while (std::isspace(*start_it))
	{
		++start_it;
		if (start_it == str.end())
			break;
	}
	while (std::isspace(*end_it))
	{
		++end_it;
		if (end_it == str.rend())
			break;
	}
	int start_pos = start_it - str.begin();
	int end_pos = end_it.base() - str.begin();
	str = start_pos <= end_pos ? std::string(start_it, end_it.base()) : "";
}

tb_int_t main(tb_int_t argc, tb_char_t** argv)
{
	const auto executable_directory = fs::path(get_program_file()).parent_path();
	const auto gitignore_files = collect_gitignore_files(executable_directory);

	std::vector<GitIgnoreMatcher> matchers;
	const auto matches = [&matchers](const fs::path& path) {
		for (const auto& matcher : matchers)
		{
			if (matcher.is_ignored(path))
			{
				return true;
			}
		}
		return false;
	};

	for (const auto& file_path : gitignore_files)
	{
		if (matches(file_path))
			continue;

		matchers.emplace_back(file_path, normalize_path(fs::current_path()));

		std::ifstream file(file_path);
		int line_num = 0;
		std::string line;

		std::set<std::string> ignore_rules;
		while (std::getline(file, line))
		{
			line_num++;
			strip(line);
			if (line == "" || line.find_first_of('#') != std::string::npos)
			{
				continue;
			}

			std::string converted_rule;





		}


	}
}