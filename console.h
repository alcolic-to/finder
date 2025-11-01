#ifndef CONSOLE_H
#define CONSOLE_H

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include "files.h"
#include "os.h"
#include "query.h"
#include "symbols.h"

inline const std::string esc = "\x1b[";

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

enum class Color { black, green, red, white, gray, term_default };

static constexpr Color black = Color::black;
static constexpr Color green = Color::green;
static constexpr Color red = Color::red;
static constexpr Color white = Color::white;
static constexpr Color gray = Color::gray;
static constexpr Color term_default = Color::term_default;

static constexpr u32 bg_color_offset = 10U; // Used only for setting default terminal bg color.

/**
 * Foreground and background color values with 256 color mode.
 */
template<Color c>
static constexpr u32 color_value()
{
    if constexpr (c == black)
        return 0;
    else if constexpr (c == green)
        return 2;
    else if constexpr (c == red)
        return 1;
    else if constexpr (c == white)
        return 7;
    else if constexpr (c == gray)
        return 237;
    else if constexpr (c == term_default)
        return 39;
    else
        static_assert(false, "Invalid color.");
}

enum class CopyOpt { file_name, file_path, full, full_quoted };

// Defines the coordinates of a character cell in a console screen buffer. The origin of the
// coordinate system (0, 0) is at the top, left cell of the buffer.
// X - the horizontal coordinate or column value.
// Y - the vertical coordinate or row value.
//
class Console {
public:
    static constexpr u32 coord_stack_size = 8U;

    static constexpr u32 col_start_pos = 1U;
    static constexpr u32 row_start_pos = 1U;

    /**
     * Minimum console size required.
     */
    static constexpr u32 min_x_required = 40U;
    static constexpr u32 min_y_required = 3U;

    struct Coord {
        u32 m_x;
        u32 m_y;
    };

    explicit Console();

    Console(const Console&) = delete;
    Console(Console&&) noexcept = delete;

    Console& operator=(const Console&) = delete;
    Console& operator=(Console&&) noexcept = delete;

    ~Console();

    void resize(os::Coordinates coord);

    Console& operator<<(const std::string& s);
    Console& operator>>(os::ConsoleInput& input);

    template<Color color_fg, Color color_bg>
    void set_color()
    {
        if (color_fg != m_color_fg) {
            if (color_fg == term_default)
                command("{}m", color_value<term_default>());
            else
                command("38;5;{}m", color_value<color_fg>());

            m_color_fg = color_fg;
        }

        if (color_bg != m_color_bg) {
            if (color_bg == term_default)
                command(";{}m", color_value<term_default>() + bg_color_offset);
            else
                command("48;5;{}m", color_value<color_bg>());

            m_color_bg = color_bg;
        }
    }

    template<Color color_fg = term_default, Color color_bg = term_default, class... Args>
    void write(std::format_string<Args...> str, Args&&... args)
    {
        set_color<color_fg, color_bg>();

        std::string fmt = std::format(str, std::forward<Args>(args)...);

        sz limit = std::min(fmt.size(), sz(m_max_x - m_x));
        std::string_view limited{fmt.begin(), fmt.begin() + limit}; // NOLINT

        m_stream.append(limited);
        m_x += limit;
    }

    template<Color color_fg = term_default, Color color_bg = term_default, class Arg>
    void write(Arg&& arg)
    {
        set_color<color_fg, color_bg>();

        std::string fmt{std::forward<Arg>(arg)}; // NOLINT

        sz limit = std::min(fmt.size(), sz(m_max_x - m_x));
        std::string_view limited{fmt.begin(), fmt.begin() + limit}; // NOLINT

        m_stream.append(limited);
        m_x += limit;
    }

    template<class... Args>
    void command(std::format_string<Args...> str, Args&&... args)
    {
        m_stream.append(esc + std::format(str, std::forward<Args>(args)...));
    }

    template<class Arg>
    void command(Arg&& arg)
    {
        m_stream.append(esc + std::forward<Arg>(arg)); // NOLINT
    }

    Console& clear();
    Console& flush();
    Console& clear_rest_of_line();
    Console& apply_cursor_pos();
    Console& set_cursor_pos(Coord coord);
    Console& set_cursor_pos(u32 x, u32 y);

    template<Direction d>
    Console& move_cursor(u32 times = 1U)
    {
        if constexpr (d == Direction::up)
            m_y = std::max(m_y - times, m_min_y);
        else if constexpr (d == Direction::down)
            m_y = std::min(m_y + times, m_max_y);
        else if constexpr (d == Direction::left)
            m_x = std::max(m_x - times, m_min_x);
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

    [[nodiscard]] u32 x() const noexcept { return m_x; }

    [[nodiscard]] u32 y() const noexcept { return m_y; }

    [[nodiscard]] Coord coord() const noexcept { return Coord{.m_x = x(), .m_y = y()}; }

    [[nodiscard]] u32 min_x() const noexcept { return m_min_x; }

    [[nodiscard]] u32 min_y() const noexcept { return m_min_y; }

    [[nodiscard]] u32 max_x() const noexcept { return m_max_x; }

    [[nodiscard]] u32 max_y() const noexcept { return m_max_y; }

    Console& push_cursor_coord();
    Console& pop_cursor_coord();

    template<bool picked = false>
    Console& print_single_search_result(const Files::Match& match, const Query& query);

    [[nodiscard]] const Files::Match& pick_result(const Files::Matches& results) const;

    Console& init_picker(const Files::Matches& results, const Query& query);

    template<Direction d>
    Console& move_picker(const Files::Matches& results, const Query& query);

    template<CopyOpt copy_opt>
    Console& copy_result_to_clipboard(const Files::Matches& results);

    Console& print_search_results(const Files::Matches& matches, const Query& query);

    Console& draw_symbol_search_results(const Symbol* symbol);

    void render_main(const Query& query, u32 cpus_count, u32 workers_count, u32 tasks_count,
                     u32 objects_count, const Files::Matches& results,
                     std::chrono::duration<long long, std::ratio<1, 1000>> time);

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
    void* m_in_handle;
    void* m_out_handle;
    u32 m_x{col_start_pos};
    u32 m_y{row_start_pos};
    u32 m_min_x = 1U;
    u32 m_min_y = 1U;
    u32 m_max_x;
    u32 m_max_y;
    std::array<Coord, coord_stack_size> m_coord_stack{};
    u32 m_stack_size = 0;
    Coord m_picker{.m_x = col_start_pos, .m_y = row_start_pos}; // ">" - file picker.
    Color m_color_fg = term_default;
    Color m_color_bg = term_default;
    std::string m_stream; // need to cache cout, because of horrible windows terminal performance.
};

#endif // CONSOLE_H
