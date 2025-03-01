#include <filesystem>
#include <iostream>
#include <string>

#include "tbox/tbox.h"

namespace fs = std::filesystem;

#ifndef TB_CONFIG_OS_WINDOWS
    #include <unistd.h>
#endif


#if defined(TB_CONFIG_OS_WINDOWS)
    #include <windows.h>
#elif defined(__COSMOPOLITAN__)
    #include <windowsesque.h>
    #include <sys/utsname.h>
#endif

// proc/self
#if defined(TB_CONFIG_OS_LINUX)
#   define XM_PROC_SELF_FILE        "/proc/self/exe"
#elif defined(TB_CONFIG_OS_BSD) && !defined(__OpenBSD__) && !defined(__FreeBSD__)
#   if defined(__NetBSD__)
#       define XM_PROC_SELF_FILE    "/proc/curproc/exe"
#   else
#       define XM_PROC_SELF_FILE    "/proc/curproc/file"
#   endif
#endif

std::string get_sys_name()
{
    tb_char_t const* name = tb_null;
#if defined(__COSMOPOLITAN__)
    struct utsname buffer;
    if (uname(&buffer) == 0)
    {
        if (tb_strstr(buffer.sysname, "Darwin"))
            name = "macosx";
        else if (tb_strstr(buffer.sysname, "Linux"))
            name = "linux";
        else if (tb_strstr(buffer.sysname, "Windows"))
            name = "windows";
    }
#elif defined(TB_CONFIG_OS_WINDOWS)
    name = "windows";
#elif defined(TB_CONFIG_OS_MACOSX)
    name = "macosx";
#elif defined(TB_CONFIG_OS_LINUX)
    name = "linux";
#endif
    return std::string(name);
}

std::string get_program_file() {

    std::string sysname = std::string(get_sys_name());
    std::string path = std::string(TB_PATH_MAXN, '\0');
    tb_size_t maxn = TB_PATH_MAXN;
    tb_bool_t ok = tb_false;

    if(sysname == "windows") {
#if defined(TB_CONFIG_OS_WINDOWS) || defined(__COSMOPOLITAN__)
        // get the executale file path as program directory
        tb_wchar_t buf[TB_PATH_MAXN] = {0};
        tb_size_t size = (tb_size_t)GetModuleFileNameW(tb_null, buf, (DWORD)TB_PATH_MAXN);
        // tb_assert_and_check_break(size < TB_PATH_MAXN);
        // end
        buf[size] = L'\0';
        size = tb_wcstombs(path.data(), buf, maxn);
        // tb_assert_and_check_break(size < maxn);
        path[size] = '\0';
        return path;
#endif
    }
    else if(sysname == "linux") {
#if defined(TB_CONFIG_OS_LINUX)
        // get the executale file path as program directory
        ssize_t size = readlink(XM_PROC_SELF_FILE, path.data(), (size_t)maxn);
        if (size > 0 && size < maxn)
        {
            path[size] = '\0';
            ok = tb_true;
        }
#endif
    } else if(sysname == "macosx") {
#if defined(TB_CONFIG_OS_MACOSX)
        /*
         * _NSGetExecutablePath() copies the path of the main executable into the buffer. The bufsize parameter
         * should initially be the size of the buffer.  The function returns 0 if the path was successfully copied,
         * and *bufsize is left unchanged. It returns -1 if the buffer is not large enough, and *bufsize is set
         * to the size required.
         *
         * Note that _NSGetExecutablePath will return "a path" to the executable not a "real path" to the executable.
         * That is the path may be a symbolic link and not the real file. With deep directories the total bufsize
         * needed could be more than MAXPATHLEN.
         */
         tb_uint32_t bufsize = (tb_uint32_t)maxn;
         if (!_NSGetExecutablePath(path.data(), &bufsize))
            ok = tb_true;
#endif
    }
    return path;
}

tb_int_t main(tb_int_t argc, tb_char_t** argv) {
    std::cout << get_sys_name() << std::endl;
    std::cout << get_program_file() << std::endl;

    auto func = [](tb_char_t const* path, tb_file_info_t const* info, tb_cpointer_t priv) -> tb_long_t {
        std::cout << path << std::endl;
        return TB_DIRECTORY_WALK_CODE_CONTINUE;
    };
    
    std::string directory = fs::path(get_program_file()).parent_path().generic_string();
    std::cout << directory << std::endl;

    tb_directory_walk(directory.c_str(), -1, true, func, nullptr);
}