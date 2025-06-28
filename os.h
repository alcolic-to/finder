#pragma once

#ifndef OS_SPECIFIC_H
#define OS_SPECIFIC_H

#include <cstddef>
#include <cstdint>

namespace os {

struct Coordinates {
    int16_t x;
    int16_t y;
};

void* init_console_handle();
Coordinates console_window_size(void* handle);
void set_console_cursor_position(void* handle, Coordinates coord);
void fill_console_line(void* handle, Coordinates coord, char ch);
void write_to_console(void* handle, const void* data, size_t size);
void console_scan(int32_t& input);

} // namespace os

#endif // OS_SPECIFIC_H
