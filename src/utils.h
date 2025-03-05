#ifndef UTILS_H
#define UTILS_H
#include "tbox/tbox.h"

#ifdef __cplusplus
    extern "C" {
#endif

tb_char_t const* get_program_file();
tb_char_t const* get_sys_name();

#ifdef __cplusplus
    }
#endif

#endif