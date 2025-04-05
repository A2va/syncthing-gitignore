#include <condition_variable>
#include <algorithm>
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

struct Config
{
	std::set<std::string> synctignore_rules;
	std::set<std::string> user_rules;
	std::map<fs::path, fs::file_time_type> gitignore_files;
	bool autostart;
	NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, synctignore_rules, user_rules, gitignore_files,
								   autostart);

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
		if (entry.path().filename() == ".gitignore")
		{
			const auto mtime = entry.last_write_time();
			gitignore_files.emplace(entry.path().lexically_normal(), mtime);
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
	tb_trace_i("[stignore] saving");
	std::ofstream ofs(".stignore", std::ios::out);
	for (const auto& rule : config.synctignore_rules)
	{
		ofs << rule << "\n";
	}

	ofs << "// USER RULES" << "\n";

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

	// Consider any rule that is isn't in synctignore_rules as user rules
	for (const auto& rule : rules)
	{

		if ((config.synctignore_rules.contains(rule) || rule.starts_with("//")) == false)
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

	// First stignore creation if it doesn't exist
	if (!fs::exists(".stignore"))
	{
		const auto executable_directory =
			normalize_path(fs::path(get_program_file()).parent_path());
		const auto gitignore_files = collect_gitignore_files(executable_directory);
		config.synctignore_rules = convert_ignore_rules(gitignore_files);
		config.gitignore_files = gitignore_files;
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

	// As a cosmocc program is compiled on linux, the file watcher relies on inotify function
	// but those are not avaivable on other platform than linux with cosmocc
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
		if ((event.event & TB_FWATCHER_EVENT_MODIFY) && (file.filename() == ".gitignore"))
		{

			tb_trace_i("[watcher] gitignore modified at : %s", file.generic_string().c_str());
			std::lock_guard<std::mutex> lock(mutex);
			// gitgnore have changed, update the rules
			auto entry = config.gitignore_files.extract(file);
			entry.mapped() = fs::last_write_time(file);

			std::map<fs::path, fs::file_time_type> m;
			m.insert(std::move(entry));
			config.synctignore_rules.merge(convert_ignore_rules(m));
			config.gitignore_files.merge(m);

			save_stignore(config);
		}
	}

	stop_thread.store(true);
	poll.join();
	tb_fwatcher_exit(fwatcher);
	return 0;
}