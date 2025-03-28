#ifndef UTILS_H
#define UTILS_H
#include "cosmocc.h"
#include <filesystem>
#include <string>

std::string get_sys_name();
std::string get_program_file();

void enable_autostart();
void disable_autostart();

bool is_running();

std::filesystem::path to_unix_path(const std::filesystem::path& path);
std::filesystem::path to_windows_path(const std::filesystem::path& path);
std::filesystem::path normalize_path(const std::filesystem::path& path);

#endif