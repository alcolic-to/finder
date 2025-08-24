#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstddef>
#include <filesystem>

#include "util.h"

namespace os {

constexpr char path_sep = std::filesystem::path::preferred_separator;

struct Coordinates {
    i16 x;
    i16 y;
};

i16 console_row_start();
i16 console_col_start();
bool is_esc(i32 input);
bool is_backspace(i32 input);

void* init_console_handle();
void close_console(void* handle);
Coordinates console_window_size(void* handle);
void set_console_cursor_position(void* handle, Coordinates coord);
void fill_console_line(void* handle, Coordinates coord, char ch);
void write_to_console(void* handle, const void* data, size_t size);
void console_scan(i32& input);

} // namespace os

#endif // OS_SPECIFIC_H
