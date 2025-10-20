#include "console.h"

#include <cstddef>
#include <iostream>
#include <string>

#include "os.h"

// We will map cursor coordinates as a windows console:
// Defines the coordinates of a character cell in a console screen buffer. The origin of the
// coordinate system (0, 0) is at the top, left cell of the buffer.
// X - the horizontal coordinate or column value.
// Y - the vertical coordinate or row value.
//
Console::Console()
    : m_handle{os::init_console_handle()}
    , m_x{static_cast<u32>(os::console_col_start())}
    , m_y{static_cast<u32>(os::console_row_start())}
{
    os::Coordinates coord = os::console_window_size(m_handle);
    m_max_x = coord.x;
    m_max_y = coord.y;
    m_picker.m_x = 0;
    m_picker.m_y = m_max_y - 2;
    m_stream.reserve(sz(m_max_x) * m_max_y);
    clear();
    flush();
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

Console& Console::getline(std::string& line)
{
    std::getline(std::cin, line);
    return *this;
}
