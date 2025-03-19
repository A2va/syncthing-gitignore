#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
#include <string>

#include <nlohmann/json.hpp>
#include <tbox/tbox.h>

#include "gitignore_parser.hpp"
#include "utils.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Config
{
	std::set<std::string> synctignore_rules;
	std::set<std::string> user_rules;
	std::set<fs::path> gitignore_files;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, synctignore_rules, user_rules, gitignore_files);

	static Config load()
	{
		Config config;
		const fs::path config_path =
			normalize_path(fs::path(get_program_file()).parent_path()) / "synctignore.json";

		if (fs::exists(config_path))
		{
			std::ifstream ifs(config_path);
			json data = json::parse(ifs);
			config = data.template get<Config>();
		}
		return config;
	}

	void save()
	{
		const fs::path config_path =
			normalize_path(fs::path(get_program_file()).parent_path()) / "synctignore.json";
		json data = *this;
		std::ofstream ofs(config_path);

		ofs << data.dump(4);
	}
};

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

std::set<fs::path> collect_gitignore_files(const fs::path& path)
{
	std::set<fs::path> gitignore_files;
	fs::recursive_directory_iterator dir_iter(normalize_path(path));
	for (const auto& entry : dir_iter)
	{
		std::cout << entry.path() << std::endl;
		if (entry.path().filename() == ".gitignore")
		{
			gitignore_files.insert(entry.path());
		}
	}
	return gitignore_files;
}

// Convert ignore rules from git to syncthing
std::set<std::string> convert_ignore_rules(const std::set<fs::path>& gitignore_files)
{
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

	std::set<std::string> ignore_rules;
	for (const auto& file_path : gitignore_files)
	{
		if (matches(file_path))
			continue;

		matchers.emplace_back(file_path, normalize_path(fs::current_path()));

		fs::path gitignore_parent_path = fs::relative(file_path.parent_path()).lexically_normal();
		std::ifstream file(file_path);
		int line_num = 0;
		std::string line;

		while (std::getline(file, line))
		{
			line_num++;
			strip(line);
			if (line == "" || line.starts_with("#"))
				continue;

			std::string negation = "";
			if (line.starts_with("!"))
			{
				negation = "!";
				line = line.substr(1);
			}

			if (line.starts_with("/"))
			{
				ignore_rules.insert(negation + gitignore_parent_path.generic_string() + line);
				continue;
			}

			if ((line.find('/') != std::string::npos) && (line.back() != '/'))
			{
				ignore_rules.insert(negation + gitignore_parent_path.generic_string() + line);
				continue;
			}

			ignore_rules.insert(negation + gitignore_parent_path.generic_string() + '/' + line);
			ignore_rules.insert(negation + gitignore_parent_path.generic_string() + '/' + "**" +
								'/' + line);
		}
	}
	return ignore_rules;
}

void save_stignore(const Config& config)
{
	std::ofstream ofs("stignore", std::ios::out);
	for (const auto& rule : config.synctignore_rules)
	{
		ofs << rule << "\n";
	}

	ofs << "# USER RULES" << "\n";

	for (const auto& rule : config.user_rules)
	{
		ofs << rule << "\n";
	}
}

tb_int_t main(tb_int_t argc, tb_char_t** argv)
{
	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());
	std::set<std::string> ignore_rules;

	// Load config
	Config config = Config::load();

	// First stignore creation if it doesn't exist
	if (!fs::exists(executable_directory / "stignore"))
	{
		const auto gitignore_files = collect_gitignore_files(executable_directory);
		config.synctignore_rules = convert_ignore_rules(gitignore_files);
		save_stignore(config);
		config.save();
	}
	else
	{
		std::ifstream ifs("stignore");
		int line_num = 0;
		std::string line;

		std::set<std::string> rules;

		while (std::getline(ifs, line))
		{
			line_num++;
			strip(line);
			rules.insert(line);
		}

		// Consider any rule that is isn't in synctignore_rules as user rules
		for (const auto& rule : rules)
		{
			if (!config.synctignore_rules.contains(rule))
			{
				config.user_rules.insert(rule);
			}
		}
		save_stignore(config);
	}

	for (const auto& rule : ignore_rules)
	{
		std::cout << rule << std::endl;
	}
}