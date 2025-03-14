#include <filesystem>
#include <iostream>
#include <string>

#include "tbox/tbox.h"
#include "utils.hpp"

namespace fs = std::filesystem;

tb_int_t main(tb_int_t argc, tb_char_t** argv) {
    auto func = [](tb_char_t const* path, tb_file_info_t const* info, tb_cpointer_t priv) -> tb_long_t {
        std::cout << path << std::endl;
        return TB_DIRECTORY_WALK_CODE_CONTINUE;
    };
    
    const auto directory = fs::path(get_program_file()).parent_path();
    tb_directory_walk(normalize_path(directory).string().c_str(), -1, true, func, nullptr);
}