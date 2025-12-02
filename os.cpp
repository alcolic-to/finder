#include "os.hpp"

#include <cerrno>
#include <csignal>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>

#include "util.hpp"

// NOLINTBEGIN(misc-include-cleaner)

namespace os {

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
    return input == 127;
}

bool is_ctrl_j(i32 input)
{
    return input == 10;
}

bool is_ctrl_k(i32 input)
{
    return input == 11;
}

bool is_ctrl_h(i32 input)
{
    return input == 8;
}

bool is_ctrl_l(i32 input)
{
    return input == 12;
}

bool is_ctrl_f(i32 input)
{
    return input == 6;
}

bool is_ctrl_i(i32 input)
{
    return input == 9;
}

bool is_ctrl_p(i32 input)
{
    return input == 16;
}

bool is_ctrl_y(i32 input)
{
    return input == 25;
}

bool is_ctrl_q(i32 input)
{
    return input == 17;
}

bool is_ctrl_u(i32 input)
{
    return input == 21;
}

bool is_ctrl_d(i32 input)
{
    return input == 4;
}

bool is_ctrl_g(i32 input)
{
    return input == 7;
}

/**
 * Used for settings restoration.
 */
static DWORD initial_in_mode;
static DWORD initial_out_mode;

void* init_console_in_handle()
{
    HANDLE handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        throw std::exception{"Failed to get console input handle."};

    GetConsoleMode(handle, &initial_in_mode);
    BOOL r = SetConsoleMode(handle, initial_in_mode | ENABLE_WINDOW_INPUT);
    if (r == 0)
        throw std::exception{"Failed to set new console mode."};

    FlushConsoleInputBuffer(handle);
    return handle;
}

void* init_console_out_handle()
{
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        throw std::exception{"Failed to get console output handle."};

    GetConsoleMode(handle, &initial_out_mode);
    BOOL r = SetConsoleMode(handle, initial_out_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                                        ENABLE_PROCESSED_OUTPUT);
    if (r == 0)
        throw std::exception{"Failed to set new console mode."};

    return handle;
}

void close_console(void* in_handle, void* out_handle) // NOLINT
{
    BOOL r = SetConsoleMode(in_handle, initial_in_mode);
    if (r == 0)
        throw std::exception{"Failed to restore console settings."};

    r = SetConsoleMode(out_handle, initial_out_mode);
    if (r == 0)
        throw std::exception{"Failed to restore console settings."};
}

Coordinates console_window_size(void* out_handle)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    BOOL r = GetConsoleScreenBufferInfo(out_handle, &csbi);
    if (r == 0)
        throw std::exception{"Could not get console screen buffer info."};

    return Coordinates{csbi.dwMaximumWindowSize.X, csbi.dwMaximumWindowSize.Y};
}

void console_scan(void* in_handle, ConsoleInput& input)
{
    while (true) {
        INPUT_RECORD record;
        DWORD count = 0;

        if (!ReadConsoleInput(in_handle, &record, 1, &count))
            throw std::exception{"Failed to read input."};

        switch (record.EventType) {
        case KEY_EVENT: {
            if (!record.Event.KeyEvent.bKeyDown)
                continue;

            if (record.Event.KeyEvent.wVirtualKeyCode == VK_BACK) {
                input = 127;
                return;
            }

            input = record.Event.KeyEvent.uChar.AsciiChar;
            return;
        }
        case WINDOW_BUFFER_SIZE_EVENT: {
            const auto& e = record.Event.WindowBufferSizeEvent;
            input = Coordinates{e.dwSize.X, e.dwSize.Y};
            return;
        }
        default: {
            continue;
        }
        }
    }
}

std::string root_dir()
{
    return "C:\\";
}

template<bool throws>
i32 exec_cmd_internal(const std::string& cmd)
{
    i32 r = system(cmd.c_str());
    if constexpr (throws) {
        if (r != 0)
            throw std::runtime_error{std::format("Failed to execute cmd: {}, error: {}", cmd, r)};
    }

    return r;
}

template<bool throws>
i32 copy_to_clipboard(const std::string& str)
{
    auto res = [&](i32 error) {
        if constexpr (throws) {
            if (error != 0)
                throw std::runtime_error{
                    std::format("Failed to copy to clipboard. Win32: {}", GetLastError())};
        }

        return error;
    };

    if (OpenClipboard(nullptr) == 0)
        return res(-1);

    if (EmptyClipboard() == 0)
        return res(-1);

    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, str.size() + 1);
    if (!mem) {
        CloseClipboard();
        return res(-1);
    }

    std::memcpy(GlobalLock(mem), str.c_str(), str.size() + 1);
    GlobalUnlock(mem);

    if (SetClipboardData(CF_TEXT, mem) == nullptr) {
        CloseClipboard();
        return res(-1);
    }

    CloseClipboard();
    return 0;
}

#elif defined(OS_LINUX)

// NOLINTBEGIN

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>

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

bool is_ctrl_j(i32 input)
{
    return input == 10;
}

bool is_ctrl_k(i32 input)
{
    return input == 11;
}

bool is_ctrl_h(i32 input)
{
    return input == 8;
}

bool is_ctrl_l(i32 input)
{
    return input == 12;
}

bool is_ctrl_f(i32 input)
{
    return input == 6;
}

bool is_ctrl_i(i32 input)
{
    return input == 9;
}

bool is_ctrl_y(i32 input)
{
    return input == 25;
}

bool is_ctrl_q(i32 input)
{
    return input == 17;
}

bool is_ctrl_u(i32 input)
{
    return input == 21;
}

bool is_ctrl_d(i32 input)
{
    return input == 4;
}

bool is_ctrl_g(i32 input)
{
    return input == 7;
}

bool is_ctrl_p(i32 input)
{
    return input == 16;
}

/**
 * Poller class user for receiving user commands.
 *
 * Since console window resize raises SIGWINCH signal, we can't simply read user input, since read
 * linux call is blocking on some systems (even when process receives a signal). Instead, we will
 * use polling mechanism. Polling can wait for events on multiple file descriptors (fd), enabling us
 * to handle both user input and signals.
 * Each time any of fds receives I/O, our poll_events() call will unblock and we will query each
 * pollfd to determine which one has a I/O ready.
 * To handle inputs from stdin, we create a standard pollfd with STDIN_FILENO and store it in
 * fds[0]. So each time user types command, our poll_events() call will unblock with I/O ready in
 * fds[0].
 * To handle signals, we use a unidirectional pipe which will be stored in fds[1].
 * Signals are handled via signal handlers, and we will write data to our pipe in our signal
 * handler, which will again unblock poll_events() call with with I/O ready in fds[1].

 * pipe man: pipe() creates a pipe, a unidirectional data channel that can be used for interprocess
 * communication. The array pipefd is used to return two file descriptors referring to the ends of
 * the pipe. pipefd[0] refers to the read end of the pipe. pipefd[1] refers to the write end of the
 * pipe. Data written to the write end of the pipe is buffered by the kernel until it is read from
 * the read end of the pipe. For further details, see pipe(7).
 */
class Poller {
public:
    static constexpr i32 inf = -1;

    Poller()
    {
        if (pipe(pipe_fd) == -1)
            throw std::runtime_error{"Failed to initialize pipe."};

        fds[0].fd = STDIN_FILENO; // keyboard input
        fds[0].events = POLLIN;
        fds[1].fd = pipe_fd[0]; // pipe reader signal notifications (SIGWINCH)
        fds[1].events = POLLIN;
    }

    void poll_events() { poll(fds, 2, inf); }

    bool stdin_received() { return fds[0].revents & POLLIN; }

    bool signal_received() { return fds[1].revents & POLLIN; }

    i32& stdin_reader() { return fds[0].fd; }

    i32& signal_reader() { return pipe_fd[0]; }

    i32& signal_writer() { return pipe_fd[1]; }

    pollfd fds[2];
    i32 pipe_fd[2];
};

static Poller poller;

/**
 * Signal handler for SIGWINCH.
 * Writes new window coordinates into the pipe.
 */
void handle_resize(int)
{
    Coordinates c = console_window_size(nullptr);
    write(poller.signal_writer(), &c, sizeof(c));
}

static termios initial_termios; // Used for settings restoration.

void* init_console_in_handle()
{
    tcgetattr(STDIN_FILENO, &initial_termios);
    termios* t = new termios{initial_termios};

    t->c_lflag &= ~(ICANON | ECHO); // NOLINT disable canonical mode and echo.
    t->c_cc[VMIN] = 1;              // min number of characters for read()
    t->c_cc[VTIME] = 0;             // no timeout

    tcsetattr(STDIN_FILENO, TCSANOW, t); // apply settings immediately

    std::signal(SIGWINCH, handle_resize);

    return t;
}

void* init_console_out_handle()
{
    return nullptr;
};

void close_console(void* in_handle, void* out_handle) // NOLINT
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

void console_scan([[maybe_unused]] void* in_handle, ConsoleInput& input)
{
    poller.poll_events();

    if (poller.stdin_received()) {
        i32 in = 0;
        if (read(poller.stdin_reader(), &in, 1) == -1) {
            std::cerr << "Failed to read input.\n";
            std::terminate();
        }

        input = in;
        return;
    }

    if (poller.signal_received()) {
        Coordinates c;
        if (read(poller.signal_reader(), &c, sizeof(c)) == -1) {
            std::cerr << "Failed to read input.\n";
            std::terminate();
        }

        input = c;
        return;
    }
}

std::string root_dir()
{
    return "/";
}

template<bool throws>
i32 exec_cmd_internal(const std::string& cmd)
{
    i32 r = system(cmd.c_str());
    if constexpr (throws) {
        if (r != 0)
            throw std::runtime_error{std::format("Failed to execute cmd: {}, error: {}", cmd, r)};
    }

    return r;
}

template<bool throws>
i32 copy_to_clipboard(const std::string& str)
{
    return exec_cmd<throws>("echo -n '{}' | xclip -selection clipboard", str);
}

// NOLINTEND

#else
static_assert(!"Unsupported OS.");
#endif

template i32 copy_to_clipboard<true>(const std::string& str);
template i32 copy_to_clipboard<false>(const std::string& str);

template i32 exec_cmd_internal<true>(const std::string& cmd);
template i32 exec_cmd_internal<false>(const std::string& cmd);

} // namespace os
