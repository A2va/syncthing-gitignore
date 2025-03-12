#ifndef UTILS_H
#define UTILS_H
#include "tbox/tbox.h"

#ifdef __cplusplus
    extern "C" {
#endif

void _get_program_file(tb_char_t* path);
void _get_sys_name(tb_char_t* path);

#ifdef __cplusplus
    }
#endif

#endif