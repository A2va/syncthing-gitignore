#include "cosmocc.h"
#include <stdio.h>
#include <uchar.h>

#ifndef TB_CONFIG_OS_WINDOWS
	#include <unistd.h>
#endif

#if defined(TB_CONFIG_OS_WINDOWS)
	#include <windows.h>
#elif defined(__COSMOPOLITAN__)
	#include <libc/proc/ntspawn.h>
	#include <sys/utsname.h>
	#include <windowsesque.h>
	#define GetModuleFileNameW GetModuleFileName
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

tb_size_t char16_wcslen(char16_t const* s)
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