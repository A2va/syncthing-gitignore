#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>

#include "cosmocc.h"
#ifndef TB_CONFIG_OS_WINDOWS
	#include <unistd.h>
	#include <fcntl.h>
	#include <sys/file.h>
#endif

#if defined(TB_CONFIG_OS_WINDOWS)
	#include <windows.h>
#elif defined(__COSMOPOLITAN__)
	#include <libc/proc/ntspawn.h>
	#include <sys/utsname.h>
	#include <windowsesque.h>
	#define LSTATUS LONG
	#define GetModuleFileNameW GetModuleFileName
	#define KEY_SET_VALUE 0x00000002 // Remove after the new cosmocc update
#endif

// proc/self
#if defined(TB_CONFIG_OS_LINUX)
	#define XM_PROC_SELF_FILE "/proc/self/exe"
#elif defined(TB_CONFIG_OS_BSD) && !defined(__OpenBSD__) && !defined(__FreeBSD__)
	#if defined(__NetBSD__)
		#define XM_PROC_SELF_FILE "/proc/curproc/exe"
	#else
		#define XM_PROC_SELF_FILE "/proc/curproc/file"
	#endif
#endif

#ifndef __COSMOPOLITAN__
static inline int IsAlpha(int c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}
// https://github.com/jart/cosmopolitan/blob/b235492e715df777bd14424dc1b7f9cd0605b379/libc/calls/mungentpath.c#L25
void mungentpath(tb_char_t* path)
{
	tb_char_t* p;

	// turn colon into semicolon
	// unless it already looks like a dos path
	for (p = path; *p; ++p)
	{
		if (p[0] == ':' && p[1] != '\\')
		{
			p[0] = ';';
		}
	}

	// turn /c/... into c:\...
	p = path;
	if (p[0] == '/' && IsAlpha(p[1]) && p[2] == '/')
	{
		p[0] = p[1];
		p[1] = ':';
	}
	for (; *p; ++p)
	{
		if (p[0] == ';' && p[1] == '/' && IsAlpha(p[2]) && p[3] == '/')
		{
			p[1] = p[2];
			p[2] = ':';
		}
	}

	// turn slash into backslash
	for (p = path; *p; ++p)
	{
		if (*p == '/')
		{
			*p = '\\';
		}
	}
}
#endif

static tb_size_t char16_wcslen(char16_t const* s)
{
	tb_assert_and_check_return_val(s, 0);

	__tb_register__ char16_t const* p = s;

#ifdef __tb_small__
	while (*p)
		p++;
	return (p - s);
#else
	while (1)
	{
		if (!p[0])
			return (p - s + 0);
		if (!p[1])
			return (p - s + 1);
		if (!p[2])
			return (p - s + 2);
		if (!p[3])
			return (p - s + 3);
		p += 4;
	}
	return 0;
#endif
}

inline static tb_size_t char16_wcstombs_charset(tb_char_t* s1, char16_t const* s2, tb_size_t n)
{
	// check
	tb_assert_and_check_return_val(s1 && s2, 0);

	// init
	tb_long_t r = 0;
	tb_size_t l = char16_wcslen(s2);

	// atow
	if (l)
	{
		tb_size_t e = (sizeof(char16_t) == 4) ? TB_CHARSET_TYPE_UTF32 : TB_CHARSET_TYPE_UTF16;
		r = tb_charset_conv_data(e | TB_CHARSET_TYPE_LE, TB_CHARSET_TYPE_UTF8, (tb_byte_t const*)s2,
								 l * sizeof(char16_t), (tb_byte_t*)s1, n);
	}

	// strip
	if (r >= 0)
		s1[r] = '\0';

	// ok?
	return r > 0 ? r : -1;
}

inline static tb_size_t char16_mbstowcs_charset(char16_t* s1, tb_char_t const* s2, tb_size_t n)
{
	// check
	tb_assert_and_check_return_val(s1 && s2, 0);

	// init
	tb_long_t r = 0;
	tb_size_t l = tb_strlen(s2);

	// mbstow
	if (l)
	{
		tb_size_t e = (sizeof(char16_t) == 4) ? TB_CHARSET_TYPE_UTF32 : TB_CHARSET_TYPE_UTF16;
		r = tb_charset_conv_data(TB_CHARSET_TYPE_UTF8, e | TB_CHARSET_TYPE_LE, (tb_byte_t const*)s2,
								 l, (tb_byte_t*)s1, n * sizeof(char16_t));

		// convert bytes to char16_t count
		if (r >= 0)
			r /= sizeof(char16_t);
	}

	// null-terminate
	if (r >= 0)
		s1[r] = 0;

	// ok?
	return r > 0 ? r : -1;
}

void _get_sys_name(tb_char_t* name)
{
#if defined(__COSMOPOLITAN__)
	struct utsname buffer;
	if (uname(&buffer) == 0)
	{
		if (tb_strstr(buffer.sysname, "Darwin"))
			tb_strcpy(name, "macosx");
		else if (tb_strstr(buffer.sysname, "Linux"))
			tb_strcpy(name, "linux");
		else if (tb_strstr(buffer.sysname, "Windows"))
			tb_strcpy(name, "windows");
	}
#elif defined(TB_CONFIG_OS_WINDOWS)
	tb_strcpy(name, "windows");
#elif defined(TB_CONFIG_OS_MACOSX)
	tb_strcpy(name, "macosx");
#elif defined(TB_CONFIG_OS_LINUX)
	tb_strcpy(name, "linux");
#endif
}

void _get_program_file(tb_char_t* path)
{

	tb_char_t sysname[16];
	_get_sys_name(sysname);
	tb_size_t maxn = TB_PATH_MAXN;
	tb_bool_t ok = tb_false;

	if (tb_strstr(sysname, "windows"))
	{
#if defined(TB_CONFIG_OS_WINDOWS) || defined(__COSMOPOLITAN__)
		// get the executale file path as program directory
	#ifdef __COSMOPOLITAN__
		char16_t buf[TB_PATH_MAXN] = {0};
		tb_size_t size =
			(tb_size_t)GetModuleFileNameW((int64_t)tb_null, (char16_t*)buf, (DWORD)TB_PATH_MAXN);
		char16_wcstombs_charset(path, buf, maxn);
	#else
		tb_wchar_t buf[TB_PATH_MAXN] = {0};
		tb_size_t size = (tb_size_t)GetModuleFileNameW(tb_null, buf, TB_PATH_MAXN);
		buf[size] = L'\0';
		size = tb_wcstombs(path, buf, maxn);
	#endif
		path[size] = '\0';
#endif
	}
	else if (tb_strstr(sysname, "linux"))
	{
#if defined(TB_CONFIG_OS_LINUX)
		// get the executale file path as program directory
		ssize_t size = readlink(XM_PROC_SELF_FILE, path, (size_t)maxn);
		if (size > 0 && size < maxn)
		{
			path[size] = '\0';
			ok = tb_true;
		}
#endif
	}
	else if (tb_strstr(sysname, "macosx"))
	{
#if defined(TB_CONFIG_OS_MACOSX)
		/*
		 * _NSGetExecutablePath() copies the path of the main executable into the buffer. The
		 * bufsize parameter should initially be the size of the buffer.  The function returns 0 if
		 * the path was successfully copied, and *bufsize is left unchanged. It returns -1 if the
		 * buffer is not large enough, and *bufsize is set to the size required.
		 *
		 * Note that _NSGetExecutablePath will return "a path" to the executable not a "real path"
		 * to the executable. That is the path may be a symbolic link and not the real file. With
		 * deep directories the total bufsize needed could be more than MAXPATHLEN.
		 */
		tb_uint32_t bufsize = (tb_uint32_t)maxn;
		if (!_NSGetExecutablePath(path, &bufsize))
			ok = tb_true;
#endif
	}
}

void _to_windows_path(tb_char_t* path)
{
	mungentpath(path);
}

int _enable_autostart(const tb_char_t* path)
{
	tb_char_t sysname[16];
	_get_sys_name(sysname);

	if (tb_strstr(sysname, "windows"))
	{
#if defined(TB_CONFIG_OS_WINDOWS) || defined(__COSMOPOLITAN__)
		HKEY hKey;
	#if defined(__COSMOPOLITAN__)
		char16_t key_path[128];
		char16_mbstowcs_charset(key_path, "synctignore", char16_wcslen(key_path));
	#else
		const char* key_path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
	#endif

		if (RegOpenKeyEx(HKEY_CURRENT_USER, key_path, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
		{
	#if defined(__COSMOPOLITAN__)
			char16_t value_name[128];
			char16_mbstowcs_charset(value_name, "synctignore", char16_wcslen(value_name));
	#else
			const char* value_name = "synctignore";
	#endif

			LSTATUS ret = RegSetValueEx(hKey, value_name, 0, REG_SZ, path, (strlen(path) + 1));
			if (ret != ERROR_SUCCESS)
			{
				tb_trace_w("Failed to add to startup program");
				return 1;
			}
			RegCloseKey(hKey);
		}
		else
		{
			tb_trace_w("Failed to open key");
			return 1;
		}
#endif
	}
	else
	{
		// Check if the program is already in the crontab.
		char check_cmd[TB_PATH_MAXN];
		snprintf(check_cmd, sizeof(check_cmd), "crontab -l 2>/dev/null | grep -q '@reboot \"%s\"'",
				 path);

		if (system(check_cmd) == 0)
		{
			return 0;
		}

		char cmd[TB_PATH_MAXN];
		// Quote the program_path so that spaces are handled correctly.
		snprintf(cmd, sizeof(cmd),
				 "(crontab -l 2>/dev/null; echo '@reboot \"%s\"') | sort -u | crontab -", path);
		if (system(cmd) != 0)
		{
			tb_trace_w("Failed to add to startup program");
			return 1;
		}
	}
	return 0;
}

int _disable_autostart(const tb_char_t* path)
{
	tb_char_t sysname[16];
	_get_sys_name(sysname);

	if (tb_strstr(sysname, "windows"))
	{
#if defined(TB_CONFIG_OS_WINDOWS) || defined(__COSMOPOLITAN__)
		HKEY hKey;
	#if defined(__COSMOPOLITAN__)
		char16_t key_path[128];
		char16_mbstowcs_charset(key_path, "synctignore", char16_wcslen(key_path));
	#else
		const char* key_path = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
	#endif
		if (RegOpenKeyEx(HKEY_CURRENT_USER, key_path, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS)
		{
	#if defined(__COSMOPOLITAN__)
			char16_t value_name[128];
			char16_mbstowcs_charset(value_name, "synctignore", char16_wcslen(value_name));
	#else
			const char* value_name = "synctignore";
	#endif
			LSTATUS ret = RegDeleteValue(hKey, value_name);
			if (ret != ERROR_SUCCESS)
			{
				tb_trace_w("Failed to remove from startup program");
				return 1;
			}
			RegCloseKey(hKey);
		}
		else
		{
			tb_trace_w("Failed to open key");
			return 1;
		}
#endif
	}
	else
	{
		int ret = system("crontab -l | grep -v 'synctignore' | crontab -");
		if (ret != 0)
		{
			tb_trace_w("Failed to remove from startup program");
			return 1;
		}
	}
	return 0;
}

tb_bool_t _is_running(const tb_char_t* path)
{
	tb_char_t sysname[16];
	_get_sys_name(sysname);

	tb_char_t uuid[37];
	tb_uuid_make_cstr(uuid, path);

	if (tb_strstr(sysname, "windows") && TB_CONFIG_OS_WINDOWS)
	{
#ifdef TB_CONFIG_OS_WINDOWS

		tb_char_t mutex_name[256];
		snprintf(mutex_name, sizeof(mutex_name), "Global\\MyUniqueProgramNameMutex_%s", uuid);

		HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\MyUniqueProgramNameMutex");
		if (hMutex == NULL)
		{
			// If the mutex cannot be created, assume an instance is running.
			return tb_true;
		}
		// If the mutex already exists, GetLastError will return ERROR_ALREADY_EXISTS.
		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			CloseHandle(hMutex);
			return tb_true;
		}

#endif
	}
	else
	{
#ifdef TB_CONFIG_OS_LINUX
		char lock_file[256];
		snprintf(lock_file, sizeof(lock_file), "/tmp/myprogram_%s.lock", uuid);

		int fd = open(lock_file, O_RDWR | O_CREAT, 0640);
		if (fd < 0)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (flock(fd, LOCK_EX | LOCK_NB) < 0)
		{
			close(fd);
			return tb_true;
		}
#endif
	}

	return tb_false;
}