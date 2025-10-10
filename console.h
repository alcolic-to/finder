#ifndef CONSOLE_H
#define CONSOLE_H

#include <array>
#include <cassert>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "os.h"
#include "symbols.h"

#define ESC "\x1b"
#define CSI "\x1b["

// Horizontal and vertical coordinate limit values.
//
enum class Direction { up, down, left, right };

static constexpr Direction up = Direction::up;
static constexpr Direction down = Direction::down;
static constexpr Direction left = Direction::left;
static constexpr Direction right = Direction::right;

enum class Edge { top, bottom, left, right };

static constexpr Edge edge_top = Edge::top;
static constexpr Edge edge_bottom = Edge::bottom;
static constexpr Edge edge_left = Edge::left;
static constexpr Edge edge_right = Edge::right;

// Defines the coordinates of a character cell in a console screen buffer. The origin of the
// coordinate system (0, 0) is at the top, left cell of the buffer.
// X - the horizontal coordinate or column value.
// Y - the vertical coordinate or row value.
//
class Console {
public:
    static constexpr u32 coord_stack_size = 8;

    struct Coord {
        u32 m_x;
        u32 m_y;
    };

    explicit Console();

    Console(const Console&) = delete;
    Console(Console&&) noexcept = delete;

    Console& operator=(const Console&) = delete;
    Console& operator=(Console&&) noexcept = delete;

    ~Console() { os::close_console(m_handle); }

    Console& operator<<(const std::string& s);
    Console& operator>>(std::string& s);
    Console& operator>>(i32& input);

    template<class... Args>
    void write(std::format_string<Args...> str, Args&&... args)
    {
        std::string fmt = std::format(str, std::forward<Args>(args)...);
        std::cout << fmt;
        m_x += fmt.size();
    }

    template<class Arg>
    void write(Arg&& arg)
    {
        std::string fmt{std::forward<Arg>(arg)};
        std::cout << fmt;
        m_x += fmt.size();
    }

    template<class... Args>
    void command(std::format_string<Args...> str, Args&&... args)
    {
        std::cout << std::format(str, std::forward<Args>(args)...);
    }

    template<class Arg>
    void command(Arg&& arg)
    {
        std::cout << std::forward<Arg>(arg); // NOLINT
    }

    Console& clear()
    {
        command(CSI "2J" CSI "H");
        set_cursor_pos(1, 1);
        return *this;
    }

    Console& flush()
    {
        std::cout.flush();
        return *this;
    }

    Console& clear_rest_of_line()
    {
        command(CSI "K");
        return *this;
    };

    Console& apply_cursor_pos()
    {
        command(CSI "{};{}H", m_y, m_x);
        return *this;
    }

    Console& set_cursor_pos(u32 x, u32 y)
    {
        m_x = x, m_y = y;
        apply_cursor_pos();

        return *this;
    }

    template<Direction d>
    Console& move_cursor(u32 times = 1U)
    {
        if constexpr (d == Direction::up)
            m_y = std::max(m_y - times, 0U);
        else if constexpr (d == Direction::down)
            m_y = std::min(m_y + times, m_max_y);
        else if constexpr (d == Direction::left)
            m_x = std::max(m_x - times, 0U);
        else if constexpr (d == Direction::right)
            m_x = std::min(m_x + times, m_max_x);
        else
            static_assert("Invalid direction.");

        apply_cursor_pos();
        return *this;
    }

    template<Edge e>
    Console& move_cursor_to()
    {
        if constexpr (e == Edge::top)
            m_y = 1;
        else if constexpr (e == Edge::bottom)
            m_y = m_max_y;
        else if constexpr (e == Edge::left)
            m_x = 1;
        else if constexpr (e == Edge::right)
            m_x = m_max_x;
        else
            static_assert("Invalid direction.");

        apply_cursor_pos();
        return *this;
    }

    Console& getline(std::string& line);

    [[nodiscard]] u32 x() const noexcept { return m_x; }

    [[nodiscard]] u32 y() const noexcept { return m_y; }

    [[nodiscard]] Coord coord() const noexcept { return Coord{x(), y()}; }

    [[nodiscard]] u32 max_x() const noexcept { return m_max_x; }

    [[nodiscard]] u32 max_y() const noexcept { return m_max_y; }

    Console& push_coord()
    {
        if (m_stack_size > coord_stack_size)
            throw std::runtime_error{"Stack is full."};

        m_coord_stack[m_stack_size++] = coord(); // NOLINT
        return *this;
    }

    Console& pop_coord()
    {
        if (m_stack_size <= 0)
            throw std::runtime_error{"Stack is empty."};

        Coord c = m_coord_stack[--m_stack_size]; // NOLINT
        m_x = c.m_x, m_y = c.m_y;

        apply_cursor_pos();
        return *this;
    }

    template<class T>
    Console& draw_search_results(const std::vector<T>& v)
    {
        move_cursor_to<edge_top>();
        move_cursor_to<edge_left>();

        auto it = v.begin();
        const auto it_end = v.end();

        while (y() != max_y() - 1) {
            if (it != it_end) {
                write((*it)->full_path());
                ++it;
            }

            clear_rest_of_line();
            move_cursor<down>();
            move_cursor_to<edge_left>();
        }

        clear_rest_of_line();
        return *this;
    }

    Console& draw_symbol_search_results(const Symbol* symbol)
    {
        move_cursor_to<edge_top>();
        move_cursor_to<edge_left>();

        if (symbol != nullptr) {
            for (const auto& symref : symbol->refs()) {
                for (const auto& line : symref.lines()) {
                    clear_rest_of_line();

                    *this << std::format("{}\\{} {}: {}\n", symref.file()->path(),
                                         symref.file()->name().c_str(), line.number(),
                                         std::string(line.preview()));

                    move_cursor<down>();
                    move_cursor_to<edge_left>();

                    if (y() == max_y() - 1)
                        return *this;
                }
            }
        }

        while (y() != max_y() - 1) {
            clear_rest_of_line();
            move_cursor<down>();
            move_cursor_to<edge_left>();
        }

        return *this;
    }

private:
    [[nodiscard]] i16 short_x() const
    {
        assert(m_x <= std::numeric_limits<i16>::max());
        return static_cast<i16>(m_x);
    }

    [[nodiscard]] i16 short_y() const
    {
        assert(m_y <= std::numeric_limits<i16>::max());
        return static_cast<i16>(m_y);
    }

    os::Coordinates os_coord() { return os::Coordinates{short_x(), short_y()}; }

private: // NOLINT
    void* m_handle;
    u32 m_x;
    u32 m_y;
    u32 m_max_x;
    u32 m_max_y;
    std::array<Coord, coord_stack_size> m_coord_stack{};
    u32 m_stack_size = 0;
};

#endif // CONSOLE_H
