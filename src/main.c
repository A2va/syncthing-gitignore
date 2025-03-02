
#include <stdio.h>
#include "tbox/tbox.h"


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

tb_char_t const* get_sys_name()
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
    return name;
}

tb_int_t main(tb_int_t argc, tb_char_t** argv) {
    printf("%s\n", get_sys_name());
}