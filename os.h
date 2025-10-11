#pragma once

#ifndef OS_H
#define OS_H

#include <cstddef>
#include <filesystem>
#include <format>

#include "types.h"

namespace os {

constexpr char path_sep = std::filesystem::path::preferred_separator;

struct Coordinates {
    i16 x;
    i16 y;
};

i16 console_row_start();
i16 console_col_start();
bool is_esc(i32 input);
bool is_term(i32 input);
bool is_backspace(i32 input);
bool is_ctrl_j(i32 input);
bool is_ctrl_k(i32 input);
bool is_ctrl_f(i32 input);
bool is_ctrl_p(i32 input);
bool is_ctrl_y(i32 input);
bool is_ctrl_u(i32 input);
bool is_ctrl_d(i32 input);

void* init_console_handle();
void close_console(void* handle);
Coordinates console_window_size(void* handle);
void set_console_cursor_position(void* handle, Coordinates coord);
void fill_console_line(void* handle, Coordinates coord, char ch);
void write_to_console(void* handle, const void* data, size_t size);
void console_scan(i32& input);

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
