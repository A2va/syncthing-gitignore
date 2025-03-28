#include <algorithm>
#include <cassert>
#include <filesystem>

#include "utils.hpp"

namespace fs = std::filesystem;
std::string get_sys_name()
{
	std::string s(10, '\0');
	_get_sys_name(s.data());
	s.shrink_to_fit();
	return s;
}

std::string get_program_file()
{
	std::string s(TB_PATH_MAXN, '\0');
	_get_program_file(s.data());
	s.shrink_to_fit();
	return s;
}

void enable_autostart()
{
	std::string program_path = get_program_file();
	int ret = _enable_autostart(program_path.c_str());
	assert(_enable_autostart(program_path.c_str()) == 0);
}

void disable_autostart()
{
	std::string program_path = get_program_file();
	int ret = _disable_autostart(program_path.c_str());
	assert(_disable_autostart(program_path.c_str()) == 0);
}

fs::path to_windows_path(const fs::path& path)
{
	std::string native_path = path.string();
	// std::string s = &path.native();
	_to_windows_path(native_path.data());
	return fs::path(native_path);
}

fs::path to_unix_path(const fs::path& path)
{
	std::string native_path = path.string();

	// Handle Windows drive letters
	if (native_path.size() > 1 && native_path[1] == ':')
	{
		char drive = std::toupper(native_path[0]); // Convert drive letter to uppercase
		native_path = '/' + std::string(1, drive) + native_path.substr(2); // Replace "C:" with "/C"
	}

	// Convert backslashes to forward slashes
	std::replace(native_path.begin(), native_path.end(), '\\', '/');

	return fs::path(native_path);
}

fs::path normalize_path(const fs::path& path)
{
#ifdef __COSMOPOLITAN__
	fs::path normalized = fs::absolute(to_unix_path(path)).lexically_normal();
#else
	fs::path normalized = fs::absolute(path).lexically_normal();
#endif

	// Convert to forward slashes for consistent matching
	std::string normalized_str = normalized.string();
	std::replace(normalized_str.begin(), normalized_str.end(), '\\', '/');
	normalized = fs::path(normalized_str);

	// Strip trailing slash/backslash if the path isn't already the root directory
	if (!normalized.empty())
	{
		if ((normalized != normalized.root_path()) && !normalized.has_filename())
		{
			normalized = normalized.parent_path();
		}
	}

	if (!path.empty() && path.extension() == ".")
	{
		fs::path filename = path.filename();
		filename += ".";
		normalized = normalized.parent_path() / filename;
	}

	return normalized;
}