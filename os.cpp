#include "os.h"

#include <exception>
#include <iostream>

#include "util.h"

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

i16 console_row_start()
{
    return 1;
};

i16 console_col_start()
{
    return 1;
};

bool is_esc(i32 input)
{
    return input == 27;
}

bool is_term(i32 input)
{
    return input == 3;
}

bool is_backspace(i32 input)
{
    return input == 8;
}

static DWORD initial_mode; // Used for settings restoration.

void* init_console_handle()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        throw std::exception{"Failed to get console output handle."};

    GetConsoleMode(handle, &initial_mode);
    BOOL r = SetConsoleMode(handle, initial_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                        ENABLE_PROCESSED_OUTPUT);
    if (r == 0)
        throw std::exception{"Failed to set new console mode."};

    return handle;
}

void close_console(void* handle)
{
    BOOL r = SetConsoleMode(handle, initial_mode);
    if (r == 0)
        throw std::exception{"Failed to restore console settings."};
}

Coordinates console_window_size(void* handle)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    BOOL r = GetConsoleScreenBufferInfo(handle, &csbi);
    if (r == 0)
        throw std::exception{"Could not get console screen buffer info."};

    return Coordinates{csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y};
}

// void get_console_cursor_position(void* handle, Coordinates coord)
// {
// CONSOLE_SCREEN_BUFFER_INFO csbi;
// if (GetConsoleScreenBufferInfo(handle, &csbi)) {
// int row = csbi.dwCursorPosition.Y + 1; // 1-based
// int col = csbi.dwCursorPosition.X + 1; // 1-based
// std::cout << "Row: " << row << " Col: " << col << "\n";
// }
// else {
// std::cerr << "Failed to get cursor position.\n";
// }
// BOOL r = (handle, to_win_coord(coord));
// if (r == 0)
// throw std::exception{"Could not apply new cursor position."};
// }

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

void write_to_console(void* handle, const void* data, sz size)
{
    unsigned long written = 0;

    WriteConsole(handle, data, size, &written, nullptr);
    if (written != size)
        throw std::exception{"Failed to write to console."};
}

void console_scan(i32& input)
{
    input = _getch();
}

std::string root_dir()
{
    return "C:\\";
}

#elif defined(OS_LINUX)

// NOLINTBEGIN

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

i16 console_row_start()
{
    return 1;
};

i16 console_col_start()
{
    return 1;
};

bool is_esc(i32 input)
{
    return input == 27;
}

bool is_term(i32 input)
{
    return input == 15;
}

bool is_backspace(i32 input)
{
    return input == 127;
}

static termios initial_termios; // Used for settings restoration.

void* init_console_handle()
{
    tcgetattr(STDIN_FILENO, &initial_termios);
    termios* t = new termios{initial_termios};

    t->c_lflag &= ~(ICANON | ECHO); // NOLINT disable canonical mode and echo.
    t->c_cc[VMIN] = 1;              // min number of characters for read()
    t->c_cc[VTIME] = 0;             // timeout (0 = no timeout)

    tcsetattr(STDIN_FILENO, TCSANOW, t); // apply settings immediately

    return t;
}

void close_console(void* handle)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &initial_termios);
}

Coordinates console_window_size([[maybe_unused]] void* handle)
{
    winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1) { // NOLINT
        std::cerr << "Failed to get console window size.\n";
        std::terminate();
    }

    return Coordinates{static_cast<i16>(w.ws_col), static_cast<i16>(w.ws_row)};
}

void set_console_cursor_position([[maybe_unused]] void* handle, Coordinates coord)
{
    std::cout << "\033[" << coord.y << ";" << coord.x << "H";
    std::cout.flush();
}

void fill_console_line(void* handle, Coordinates coord, char ch)
{
    Coordinates window_size = console_window_size(handle);
    set_console_cursor_position(handle, {console_col_start(), coord.y});

    for (i16 i = 0; i < window_size.x; ++i)
        std::cout << ch;

    std::cout.flush();

    set_console_cursor_position(handle, {console_col_start(), coord.y});
}

void write_to_console([[maybe_unused]] void* handle, const void* data, sz data_size)
{
    for (sz i = 0; i < data_size; ++i)
        std::cout << static_cast<const char*>(data)[i];

    std::cout.flush();
}

void console_scan(i32& input)
{
    input = 0;
    if (read(STDIN_FILENO, &input, 1) == -1) {
        std::cerr << "Failed to read input.\n";
        std::terminate();
    }
}

std::string root_dir()
{
    return "/";
}

// NOLINTEND

#else
static_assert(!"Unsupported OS.");
#endif

} // namespace os
