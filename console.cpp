#include "console.h"

#include <iostream>
#include <string>

// We will map cursor coordinates as a windows console:
// Defines the coordinates of a character cell in a console screen buffer. The origin of the
// coordinate system (0, 0) is at the top, left cell of the buffer.
// X - the horizontal coordinate or column value.
// Y - the vertical coordinate or row value.
//
Cursor::Cursor(void* handle) : m_handle{handle}, m_x{0}, m_y{0}
{
    os::Coordinates coord = os::console_window_size(handle);
    m_max_x = coord.x - 1;
    m_max_y = coord.y - 1;

    apply();
}

void Cursor::set_pos(uint32_t x, uint32_t y)
{
    m_x = x, m_y = y;
    apply();
}

void Cursor::apply()
{
    os::set_console_cursor_position(m_handle, os_coord());
}

Console::Console() : m_handle{os::init_console_handle()}, m_cursor{m_handle} {}

Console& Console::operator<<(const std::string& s)
{
    os::write_to_console(m_handle, static_cast<const void*>(s.data()), s.size());
    m_cursor.move<d_right, false>(s.size());
    return *this;
}

Console& Console::operator>>(std::string& s)
{
    std::cin >> s;
    return *this;
}

Console& Console::operator>>(int32_t& input)
{
    os::console_scan(input);
    return *this;
}

Console& Console::fill_line(char ch)
{
    os::fill_console_line(m_handle, m_cursor.os_coord(), ch);
    return *this;
}

Console& Console::getline(std::string& line)
{
    std::getline(std::cin, line);
    return *this;
}

Console& Console::clear()
{
    m_cursor.move_to<e_top>();
    m_cursor.move_to<e_left>();

    while (true) {
        fill_line(' ');

        if (m_cursor.y() == m_cursor.max_y())
            break;

        m_cursor.move<d_down>();
    }

    m_cursor.move_to<e_top>();
    m_cursor.move_to<e_left>();

    return *this;
}
