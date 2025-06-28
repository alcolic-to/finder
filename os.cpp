#include "os.h"

#include <exception>

// NOLINTBEGIN(misc-include-cleaner)

namespace os {

// OS specific preprocessor definitions.
//
#if defined _WIN32
#define OS_WINDOWS
#elif defined __linux__
#define OS_LINUX
#else
#define OS_UNKNOWN
#endif

// Windows implementations.
//
#if defined(OS_WINDOWS)

// Reduce size of windows.h includes and include windows.
// NOLINTBEGIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max

#include <conio.h>

// NOLINTEND
COORD to_win_coord(Coordinates coord)
{
    return COORD{coord.x, coord.y};
}

void* init_console_handle()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        throw std::exception{"Failed to get console output handle."};

    return handle;
}

Coordinates console_window_size(void* handle)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    BOOL r = GetConsoleScreenBufferInfo(handle, &csbi);
    if (r == 0)
        throw std::exception{"Could not get console screen buffer info."};

    return Coordinates{csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y};
}

void set_console_cursor_position(void* handle, Coordinates coord)
{
    BOOL r = SetConsoleCursorPosition(handle, to_win_coord(coord));
    if (r == 0)
        throw std::exception{"Could not apply new cursor position."};
}

void fill_console_line(void* handle, Coordinates coord, char ch)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(handle, &csbi);
    DWORD written = 0;

    FillConsoleOutputCharacter(handle, ch, csbi.dwSize.X, to_win_coord(coord), &written);
    if (written != csbi.dwSize.X)
        throw std::exception{"Failed to fill console line"};

    written = 0;
    FillConsoleOutputAttribute(handle, csbi.wAttributes, csbi.dwSize.X, to_win_coord(coord),
                               &written);
    if (written != csbi.dwSize.X)
        throw std::exception{"Failed to fill console line"};
}

void write_to_console(void* handle, const void* data, size_t size)
{
    unsigned long written = 0;

    WriteConsole(handle, data, size, &written, nullptr);
    if (written != size)
        throw std::exception{"Failed to write to console."};
}

void console_scan(int32_t& input)
{
    input = _getch();
}

#elif defined(OS_LINUX)

void* init_console_handle()
{
    return nullptr;
}

Coordinates console_window_size(void* handle)
{
    return {};
}

void set_console_cursor_position(void* handle, Coordinates coord)
{
    return;
}

void fill_console_line(void* handle, Coordinates coord, char ch)
{
    return;
}

void write_to_console(void* handle, const void* data, size_t size)
{
    return;
}

void console_scan(int32_t& input)
{
    return;
}

#else
static_assert(!"Unsupported OS.");
#endif

} // namespace os
