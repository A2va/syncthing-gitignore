#include <filesystem>
#include <iostream>
#include <string>

#include "tbox/tbox.h"
#include "utils.h"

namespace fs = std::filesystem;

std::string get_sys_name() {
    std::string s(10, '\0');
    _get_sys_name(s.data());
    s.shrink_to_fit();
    return s;
}

std::string get_program_file() {
    std::string s(TB_PATH_MAXN, '\0');
    _get_program_file(s.data());
    s.shrink_to_fit();
    return s;
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