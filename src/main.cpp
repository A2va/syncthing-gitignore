#include <chrono>
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

namespace nlohmann
{
	template<> struct adl_serializer<std::filesystem::file_time_type>
	{
		static void to_json(json& j, const std::filesystem::file_time_type& time)
		{
			// Store the time as duration since epoch
			auto duration = time.time_since_epoch();
			auto count = duration.count();
			j = count;
		}

		static void from_json(const json& j, std::filesystem::file_time_type& time)
		{
			// Read the duration count from JSON
			auto count = j.get<typename std::filesystem::file_time_type::duration::rep>();

			// Create a duration and then a time_point from it
			typename std::filesystem::file_time_type::duration duration(count);
			time = std::filesystem::file_time_type(duration);
		}
	};
} // namespace nlohmann

struct Config
{
	std::set<std::string> synctignore_rules;
	std::set<std::string> user_rules;
	std::map<fs::path, fs::file_time_type> gitignore_files;
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

std::map<fs::path, fs::file_time_type> collect_gitignore_files(const fs::path& path)
{
	std::map<fs::path, fs::file_time_type> gitignore_files;
	fs::recursive_directory_iterator dir_iter(normalize_path(path));
	for (const auto& entry : dir_iter)
	{
		std::cout << entry.path() << std::endl;
		if (entry.path().filename() == ".gitignore")
		{
			const auto mtime = entry.last_write_time();
			gitignore_files.emplace(entry.path(), mtime);
		}
	}
	return gitignore_files;
}

// Convert ignore rules from git to syncthing
std::set<std::string> convert_ignore_rules(
	const std::map<fs::path, fs::file_time_type>& gitignore_files)
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
	for (const auto& file : gitignore_files)
	{
		const auto file_path = file.first;
		if (matches(file_path))
			continue;

		matchers.emplace_back(file_path, normalize_path(fs::current_path()));

		fs::path gitignore_parent_path = fs::relative(file_path.parent_path()).lexically_normal();
		std::ifstream ifs(file_path);
		int line_num = 0;
		std::string line;

		while (std::getline(ifs, line))
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
	std::ofstream ofs(".stignore", std::ios::out);
	for (const auto& rule : config.synctignore_rules)
	{
		ofs << rule << "\n";
	}

	ofs << "# USER RULES"
		<< "\n";

	for (const auto& rule : config.user_rules)
	{
		ofs << rule << "\n";
	}
}

tb_int_t main(tb_int_t argc, tb_char_t** argv)
{
	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());

	// Load config
	Config config = Config::load();

	// First stignore creation if it doesn't exist
	if (!fs::exists(".stignore"))
	{
		const auto gitignore_files = collect_gitignore_files(executable_directory);
		config.synctignore_rules = convert_ignore_rules(gitignore_files);
		config.gitignore_files = gitignore_files;
		save_stignore(config);
		config.save();
	}
	else
	{
		std::ifstream ifs(".stignore");
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

		// Check if some gitignore files were modified, update stignore rules accordingly
		const auto gitignore_files = collect_gitignore_files(executable_directory);
		std::map<fs::path, fs::file_time_type> updated_gitignore;
		for (const auto& file : gitignore_files)
		{

			const auto existing_file_entry = config.gitignore_files.find(file.first);
			// New discovered file
			if (existing_file_entry == config.gitignore_files.end())
			{
				updated_gitignore.insert(file);
				continue;
			}

			// File was modified
			if (file.second > existing_file_entry->second)
			{
				updated_gitignore.insert(file);
				config.gitignore_files.erase(existing_file_entry);
			}
		}

		// Update the config with the updated files/ rules
		config.synctignore_rules.merge(convert_ignore_rules(updated_gitignore));
		config.gitignore_files.merge(updated_gitignore);

		config.save();
		save_stignore(config);
	}
}