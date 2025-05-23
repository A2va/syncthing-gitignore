#include <condition_variable>
#include <algorithm>
#include <optional>
#include <atomic>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

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

struct GitIgnoreFile
{
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(GitIgnoreFile, mtime, st_rules);
	fs::file_time_type mtime;
	std::set<std::string> st_rules;
};

struct Config
{
	//std::set<std::string> synctignore_rules;
	std::set<std::string> user_rules;
	std::map<fs::path, GitIgnoreFile> gitignore_files;
	bool autostart;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, user_rules, gitignore_files,
								   autostart);

	size_t gitignore_size;
	std::vector<GitIgnoreMatcher> matchers_vector;

	static Config load()
	{
		Config config;
		config.autostart = false;
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

	void save() const
	{
		const fs::path config_path =
			normalize_path(fs::path(get_program_file()).parent_path()) / "synctignore.json";
		json data = *this;
		std::ofstream ofs(config_path);

		ofs << data.dump(4);
	}

	std::set<std::string> st_rules() const
	{
		std::set<std::string> rules;
		for (const auto& file : gitignore_files)
		{
			rules.insert(file.second.st_rules.cbegin(), file.second.st_rules.cend());
		}
		return rules;
	}

	/// Returns all the matcher 
	std::vector<GitIgnoreMatcher> matchers(std::optional<fs::path> gitignore)
	{
		if (gitignore_files.size() == gitignore_size)
		{
			return matchers_vector;
		}

		matchers_vector = {};
		const auto executable_directory =
			normalize_path(fs::path(get_program_file()).parent_path());
		for (const auto [key, value] : gitignore_files)
		{
			if (gitignore.has_value() && (gitignore.value().compare(key))) {
				matchers_vector.emplace_back(gitignore.value(), executable_directory);
			}
		}
		gitignore_size = gitignore_files.size();
		return matchers_vector;
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

std::map<fs::path, GitIgnoreFile> collect_gitignore_files(const fs::path& path)
{
	std::map<fs::path, GitIgnoreFile> gitignore_files;
	fs::recursive_directory_iterator dir_iter(normalize_path(path));
	for (const auto& entry : dir_iter)
	{
		if (entry.path().filename() == ".gitignore")
		{
			const auto mtime = entry.last_write_time();
			gitignore_files.emplace(entry.path().lexically_normal(),
									GitIgnoreFile{.mtime = mtime, .st_rules = {}});
		}
	}
	return gitignore_files;
}

// Check if the path is ignored by a collection of GitIgnoreMatcher
bool is_path_ignored_by_any(const fs::path& path, const std::vector<GitIgnoreMatcher>& matchers)
{
	for (const auto& matcher : matchers)
	{
		if (matcher.is_ignored(path))
		{
			return true;
		}
	}
	return false;
}

// Convert ignore rules from git to syncthing
void convert_ignore_rules(const fs::path& file_path, GitIgnoreFile& gitignorefile,
						  const std::vector<GitIgnoreMatcher>& matchers)
{
	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());
	fs::path gitignore_parent_path =
		fs::relative(file_path.parent_path(), executable_directory).lexically_normal();
	// If the relative path is just ".", make it an empty path
	if (gitignore_parent_path == ".")
		gitignore_parent_path = "";

	std::ifstream ifs(file_path);
	int line_num = 0;
	std::string line;

	while (std::getline(ifs, line))
	{
		line_num++;
		strip(line);
		if (line == "" || line.starts_with("#"))
			continue;

		auto& ignore_rules = gitignorefile.st_rules;

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
			std::string sep = line.starts_with("/") ? "" : "/";
			ignore_rules.insert(negation + gitignore_parent_path.generic_string() + sep + line);
			continue;
		}

		ignore_rules.insert(negation + gitignore_parent_path.generic_string() + '/' + line);
		ignore_rules.insert(negation + gitignore_parent_path.generic_string() + '/' + "**" + '/' +
							line);
	}
}

void convert_ignore_rules(std::map<fs::path, GitIgnoreFile>& gitignore_files,
						  const std::vector<GitIgnoreMatcher>& matchers)
{
	for (auto& [file_path, gitignore_file] : gitignore_files)
	{
		convert_ignore_rules(file_path, gitignore_file, matchers);
	}
}

void save_stignore(const Config& config)
{
	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());

	tb_trace_i("[stignore] saving");
	std::ofstream ofs(executable_directory / ".stignore", std::ios::out);

	const auto rules = config.st_rules();
	for (const auto& rule : rules)
	{
		ofs << rule << "\n";
	}

	ofs << "// USER RULES"
		<< "\n";

	for (const auto& rule : config.user_rules)
	{
		ofs << rule << "\n";
	}
	config.save();
}

void load_stignore(Config& config)
{
	tb_trace_i("[stignore] loading");
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

	const auto synctignore_rules = config.st_rules();
	// Consider any rule that is isn't in synctignore_rules as user rules
	for (const auto& rule : rules)
	{

		if ((synctignore_rules.contains(rule) || rule.starts_with("//")) == false)
		{
			config.user_rules.insert(rule);
		}
	}
}

void update_stignore(Config& config)
{
	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());

	// Check if some gitignore files were modified, update stignore rules accordingly
	const auto gitignore_files = collect_gitignore_files(executable_directory);
	std::map<fs::path, GitIgnoreFile> updated_gitignore;

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
		if (file.second.mtime > existing_file_entry->second.mtime)
		{
			updated_gitignore.insert(file);
			// TODO Try to find a better way than deleting the key to put it back in the merge later
			config.gitignore_files.erase(existing_file_entry);
		}
	}

	// Remove deleted files
	for (auto it = config.gitignore_files.begin(); it != config.gitignore_files.end();)
	{
		if (!gitignore_files.contains(it->first))
		{
			it = config.gitignore_files.erase(it); // erase returns next iterator
		}
		else
		{
			++it;
		}
	}

	// Update the config with the updated files/ rules
	convert_ignore_rules(updated_gitignore, config.matchers(std::nullopt));
	config.gitignore_files.merge(updated_gitignore);

	save_stignore(config);
}

tb_int_t main(tb_int_t argc, tb_char_t** argv)
{

	if (!tb_init(tb_null, tb_null))
		return -1;

	if (is_running())
	{
		tb_trace_i("[client] Already running");
		// Raise a signal for the already running instance
		tb_socket_ref_t sock = tb_socket_init(TB_SOCKET_TYPE_TCP, TB_IPADDR_FAMILY_IPV4);

		tb_ipaddr_t addr;
		tb_ipaddr_set(&addr, "127.0.0.1", 8484, TB_IPADDR_FAMILY_IPV4);

		tb_long_t ok;
		while (!(ok = tb_socket_connect(sock, &addr)))
		{
			// wait it
			if (tb_socket_wait(sock, TB_SOCKET_EVENT_CONN, -1) <= 0)
				break;
		}

		tb_trace_i("[client] connected");

		tb_byte_t data[16];
		const char* str = "reload";
		std::copy(str, str + sizeof(str), data);

		tb_trace_i("[client] send");
		tb_socket_send(sock, data, 16);

		return 0;
	}

	// Load config
	Config config = Config::load();
	if (config.autostart)
	{
		enable_autostart();
	}

	std::set<fs::path> watched_dirs;

	const auto executable_directory = normalize_path(fs::path(get_program_file()).parent_path());
	// First stignore creation if it doesn't exist
	if (!fs::exists(executable_directory / ".stignore"))
	{
		config.gitignore_files = collect_gitignore_files(executable_directory);
		convert_ignore_rules(config.gitignore_files, config.matchers(std::nullopt));
		save_stignore(config);
	}
	else
	{
		load_stignore(config);
		update_stignore(config);
	}

	if (argc > 1)
	{
		std::string arg1 = std::string(argv[1]);
		if (arg1 == "nowatch" || arg1 == "nw")
		{
			tb_trace_i("[nowatch] quit without watching");
			return 0;
		}
	}

	// Setup file watcher
	tb_fwatcher_ref_t fwatcher = tb_fwatcher_init();

	std::mutex mutex;
	std::atomic<bool> stop_thread;

	std::thread poll([&mutex, &config, &stop_thread, &watched_dirs, fwatcher] {
		tb_socket_ref_t sock = tb_socket_init(TB_SOCKET_TYPE_TCP, TB_IPADDR_FAMILY_IPV4);
		tb_assert_and_check_return(sock);

		tb_ipaddr_t addr;
		tb_ipaddr_set(&addr, "127.0.0.1", 8484, TB_IPADDR_FAMILY_IPV4);

		tb_trace_i("[server] bind");
		tb_check_return(tb_socket_bind(sock, &addr));

		tb_trace_i("[server] listening on port 8484");
		tb_check_return(tb_socket_listen(sock, 10));

		while (!stop_thread.load())
		{
			bool reload = false;
			tb_socket_ref_t client = tb_null;
			while (1)
			{
				// accept and start client connection
				if ((client = tb_socket_accept(sock, tb_null)))
				{
					tb_trace_i("[server] accept incoming client");

					tb_socket_wait(client, TB_SOCKET_EVENT_RECV, -1);
					tb_byte_t data[16];

					tb_trace_i("[server] recieve data from client");
					tb_socket_recv(client, data, 16);
					tb_trace_d("[server] data: %s", data);

					if (!tb_strcmp(reinterpret_cast<tb_char_t*>(data), "reload"))
					{
						reload = true;
						break;
					}

					tb_socket_exit(client);
				}
				else if (tb_socket_wait(sock, TB_SOCKET_EVENT_ACPT, -1) <= 0)
				{
					break;
				}
			}

			if (reload)
			{
				std::unique_lock<std::mutex> lock(mutex);

				tb_trace_i("[reload] user requested a new scan");
				update_stignore(config);

				for (const auto& file : config.gitignore_files)
				{
					const auto directory = file.first.parent_path();
					if (!watched_dirs.contains(directory))
					{
						tb_fwatcher_add(fwatcher, directory.generic_string().c_str(), tb_false);
						watched_dirs.insert(directory);
					}
				}

				tb_fwatcher_spak(fwatcher);

				lock.unlock();
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(100ms);
				reload = false;
			}
		}
	});

	// As a cosmocc program is compiled on linux, the file watcher relies on inotify function
	// but those are not available on other platform than linux with cosmocc
	// So disable the file watching altogether.
	// https://github.com/jart/cosmopolitan/blob/5eb7cd664393d8a3420cbfe042cfc3d7c7b2670d/libc/sysv/syscalls.sh#L270
#ifdef __COSMOPOLITAN__
	return 0;
#endif

	for (const auto& file : config.gitignore_files)
	{
		const auto directory = file.first.parent_path();
		tb_assert_and_check_return_val(
			tb_fwatcher_add(fwatcher, directory.generic_string().c_str(), tb_false), tb_true);
		watched_dirs.insert(directory);
	}

	tb_bool_t eof = tb_false;
	tb_fwatcher_event_t event;
	while (!eof && tb_fwatcher_wait(fwatcher, &event, -1) >= 0)
	{
		if (tb_strstr(event.filepath, "eof"))
			eof = tb_true;

		const fs::path file = fs::path(event.filepath).lexically_normal();
		const bool is_gitignore = file.filename() == ".gitignore";
		if ((event.event & TB_FWATCHER_EVENT_MODIFY) && is_gitignore)
		{

			tb_trace_i("[watcher] gitignore modified at : %s", file.generic_string().c_str());
			std::lock_guard<std::mutex> lock(mutex);

			// gitignore have changed, update the rules
			auto it = config.gitignore_files.find(file);
			tb_assert(it != config.gitignore_files.end());

			convert_ignore_rules(config.gitignore_files, config.matchers(std::make_optional(file)));
			save_stignore(config);
		}
		else if ((event.event & TB_FWATCHER_EVENT_DELETE) && is_gitignore)
		{
			std::lock_guard<std::mutex> lock(mutex);
			// deleted file
			const auto it = config.gitignore_files.find(file);
			tb_assert(it != config.gitignore_files.end());
			config.gitignore_files.erase(it);

			save_stignore(config);
		}
	}

	stop_thread.store(true);
	poll.join();
	tb_fwatcher_exit(fwatcher);
	return 0;
}