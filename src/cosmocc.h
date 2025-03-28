#ifndef COSMOCC_UTILS_H
#define COSMOCC_UTILS_H
#include "tbox/tbox.h"

#ifdef __cplusplus
extern "C"
{
#endif

	void _get_program_file(tb_char_t* path);
	void _get_sys_name(tb_char_t* path);
	void _to_windows_path(tb_char_t* path);

	int _enable_autostart(const tb_char_t* path);
	int _disable_autostart(const tb_char_t* path);

	tb_bool_t _is_running(const tb_char_t* path);

#ifdef __cplusplus
}
#endif

#endif