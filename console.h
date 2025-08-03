#ifndef CONSOLE_H
#define CONSOLE_H

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <vector>

#include "os.h"
#include "symbols.h"

// Horizontal and vertical coordinate limit values.
//
enum class Direction { up, down, left, right };

static constexpr Direction d_up = Direction::up;
static constexpr Direction d_down = Direction::down;
static constexpr Direction d_left = Direction::left;
static constexpr Direction d_right = Direction::right;

enum class Edge { top, bottom, left, right };

static constexpr Edge e_top = Edge::top;
static constexpr Edge e_bottom = Edge::bottom;
static constexpr Edge e_left = Edge::left;
static constexpr Edge e_right = Edge::right;

// We will map cursor coordinates as a windows console:
// Defines the coordinates of a character cell in a console screen buffer. The origin of the
// coordinate system (0, 0) is at the top, left cell of the buffer.
// X - the horizontal coordinate or column value.
// Y - the vertical coordinate or row value.
//
class Cursor {
public:
    friend class Console;

    explicit Cursor(void* handle);

    template<Direction d, bool Apply = true>
    void move(uint32_t times = 1U)
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

        if constexpr (Apply)
            apply();
    }

    template<Edge e, bool Apply = true>
    void move_to()
    {
        if constexpr (e == Edge::top)
            m_y = os::console_row_start();
        else if constexpr (e == Edge::bottom)
            m_y = m_max_y;
        else if constexpr (e == Edge::left)
            m_x = os::console_col_start();
        else if constexpr (e == Edge::right)
            m_x = m_max_x;
        else
            static_assert("Invalid direction.");

        if constexpr (Apply)
            apply();
    }

    void set_pos(uint32_t x, uint32_t y);

    [[nodiscard]] uint32_t x() const noexcept { return m_x; }

    [[nodiscard]] uint32_t y() const noexcept { return m_y; }

    [[nodiscard]] uint32_t max_x() const noexcept { return m_max_x; }

    [[nodiscard]] uint32_t max_y() const noexcept { return m_max_y; }

private:
    void apply();

    [[nodiscard]] int16_t short_x() const
    {
        assert(m_x <= std::numeric_limits<int16_t>::max());
        return static_cast<int16_t>(m_x);
    }

    [[nodiscard]] int16_t short_y() const
    {
        assert(m_y <= std::numeric_limits<int16_t>::max());
        return static_cast<int16_t>(m_y);
    }

    os::Coordinates os_coord() { return os::Coordinates{short_x(), short_y()}; }

    void* m_handle;
    uint32_t m_x;
    uint32_t m_y;
    uint32_t m_max_x;
    uint32_t m_max_y;
};

class Console {
public:
    explicit Console();

    ~Console() { os::close_console(m_handle); }

    Console& operator<<(const std::string& s);
    Console& operator>>(std::string& s);
    Console& operator>>(int32_t& input);

    Console& clear();
    Console& fill_line(char ch);
    Console& getline(std::string& line);

    template<class T>
    Console& draw_search_results(const std::vector<T>& v)
    {
        m_cursor.move_to<e_top>();
        m_cursor.move_to<e_left>();

        for (auto& it : v) {
            fill_line(' ');
            *this << it->full_path();

            m_cursor.move<d_down>();
            m_cursor.move_to<e_left>();
            if (m_cursor.y() == m_cursor.max_y() - 1)
                break;
        }

        while (m_cursor.y() != m_cursor.max_y() - 1) {
            fill_line(' ');
            m_cursor.move<d_down>();
            m_cursor.move_to<e_left>();
        }

        return *this;
    }

    Console& draw_symbol_search_results(const Symbol* symbol)
    {
        m_cursor.move_to<e_top>();
        m_cursor.move_to<e_left>();

        if (symbol != nullptr) {
            for (const auto& symref : symbol->refs()) {
                for (const auto& line : symref.lines()) {
                    fill_line(' ');

                    *this << std::format("{}\\{} {}: {}\n", symref.file()->path(),
                                         symref.file()->name(), line.number(),
                                         std::string(line.preview()));

                    m_cursor.move<d_down>();
                    m_cursor.move_to<e_left>();

                    if (m_cursor.y() == m_cursor.max_y() - 1)
                        return *this;
                }
            }
        }

        while (m_cursor.y() != m_cursor.max_y() - 1) {
            fill_line(' ');
            m_cursor.move<d_down>();
            m_cursor.move_to<e_left>();
        }

        return *this;
    }

    Cursor& cursor() { return m_cursor; }

private:
    void* m_handle;
    Cursor m_cursor;
};

#endif // CONSOLE_H
