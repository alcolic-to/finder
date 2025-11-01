#pragma once

#ifndef OS_H
#define OS_H

#include <cstddef>
#include <filesystem>
#include <format>

#include "types.h"

namespace os {

constexpr char path_sep = std::filesystem::path::preferred_separator;
const inline std::string path_sep_str = std::string{path_sep}; // NOLINT

struct Coordinates {
    i16 x;
    i16 y;
};

using ConsoleInput = std::variant<os::Coordinates, i32>;

bool is_esc(i32 input);
bool is_term(i32 input);
bool is_backspace(i32 input);
bool is_ctrl_j(i32 input);
bool is_ctrl_k(i32 input);
bool is_ctrl_h(i32 input);
bool is_ctrl_l(i32 input);
bool is_ctrl_f(i32 input);
bool is_ctrl_i(i32 input);
bool is_ctrl_p(i32 input);
bool is_ctrl_y(i32 input);
bool is_ctrl_u(i32 input);
bool is_ctrl_d(i32 input);
bool is_ctrl_g(i32 input);

void* init_console_handle();
void close_console(void* handle);
Coordinates console_window_size(void* handle);
void console_scan(ConsoleInput& input);

std::string root_dir();

template<bool throws = true>
i32 copy_to_clipboard(const std::string& str);

template<bool throws = true>
i32 exec_cmd_internal(const std::string& cmd);

template<bool throws = true, class... Args>
i32 exec_cmd(const std::format_string<Args...>& str, Args&&... args)
{
    std::string cmd = std::format(str, std::forward<Args>(args)...);
    return exec_cmd_internal<throws>(cmd);
}

} // namespace os

#endif // OS_H
