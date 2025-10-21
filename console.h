#ifndef CONSOLE_H
#define CONSOLE_H

#include <algorithm>
#include <array>
#include <cassert>
#include <format>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "files.h"
#include "os.h"
#include "symbols.h"

#define ESC "\x1b["

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

enum class Color { green, red, term_default };

static constexpr Color green = Color::green;
static constexpr Color red = Color::red;
static constexpr Color term_default = Color::term_default;

template<Color c>
static constexpr u32 color_value()
{
    if constexpr (c == green)
        return 32;
    else if constexpr (c == red)
        return 31;
    else if constexpr (c == term_default)
        return 0;
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

    ~Console()
    {
        write<term_default>(""); // Reset color.
        flush();
        os::close_console(m_handle);
    }

    Console& operator<<(const std::string& s);
    Console& operator>>(std::string& s);
    Console& operator>>(i32& input);

    template<Color color = term_default, class... Args>
    void write(std::format_string<Args...> str, Args&&... args)
    {
        if (color != m_color) {
            m_stream.append(std::format(ESC "{}m", color_value<color>()));
            m_color = color;
        }

        std::string fmt = std::format(str, std::forward<Args>(args)...);
        m_stream.append(fmt);
        m_x += fmt.size();
    }

    template<Color color = term_default, class Arg>
    void write(Arg&& arg)
    {
        if (color != m_color) {
            m_stream.append(std::format(ESC "{}m", color_value<color>()));
            m_color = color;
        }

        std::string fmt{std::forward<Arg>(arg)}; // NOLINT
        m_stream.append(fmt);
        m_x += fmt.size();
    }

    template<class... Args>
    void command(std::format_string<Args...> str, Args&&... args)
    {
        m_stream.append(std::format(str, std::forward<Args>(args)...));
    }

    template<class Arg>
    void command(Arg&& arg)
    {
        m_stream.append(arg); // NOLINT
    }

    Console& clear()
    {
        command(ESC "2J" ESC "H");
        set_cursor_pos(1, 1);
        return *this;
    }

    Console& flush()
    {
        std::cout << m_stream;
        std::cout.flush();
        m_stream.clear();

        return *this;
    }

    Console& clear_rest_of_line()
    {
        command(ESC "K");
        return *this;
    };

    Console& apply_cursor_pos()
    {
        command(ESC "{};{}H", m_y, m_x);
        return *this;
    }

    Console& set_cursor_pos(Coord coord) { return set_cursor_pos(coord.m_x, coord.m_y); }

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

    Console& getline(std::string& line);

    [[nodiscard]] u32 x() const noexcept { return m_x; }

    [[nodiscard]] u32 y() const noexcept { return m_y; }

    [[nodiscard]] Coord coord() const noexcept { return Coord{.m_x = x(), .m_y = y()}; }

    [[nodiscard]] u32 min_x() const noexcept { return 1; } // NOLINT

    [[nodiscard]] u32 min_y() const noexcept { return 1; } // NOLINT

    [[nodiscard]] u32 max_x() const noexcept { return m_max_x; }

    [[nodiscard]] u32 max_y() const noexcept { return m_max_y; }

    Console& push_cursor_coord()
    {
        if (m_stack_size > coord_stack_size)
            throw std::runtime_error{"Stack is full."};

        m_coord_stack[m_stack_size++] = coord(); // NOLINT
        return *this;
    }

    Console& pop_cursor_coord()
    {
        if (m_stack_size <= 0)
            throw std::runtime_error{"Stack is empty."};

        Coord c = m_coord_stack[--m_stack_size]; // NOLINT
        m_x = c.m_x, m_y = c.m_y;

        apply_cursor_pos();
        return *this;
    }

    /**
     * Draws single search result on a current line.
     */
    Console& draw_single_search_result(std::vector<Files::Match>::const_iterator it)
    {
        auto bs = it->m_match_bs;
        const FileInfo* file = it->m_file;

        std::string print = file->full_path();
        for (sz i = 0; i < print.size(); ++i)
            if (i < bs.size() && bs.test(i))
                write<green>(print[i]);
            else
                write(print[i]);

        return *this;
    }

    /**
     * Initializes picker.
     * It first deletes current picker from console, and, if search results exist, shows picker on
     * initial position.
     */
    Console& init_picker(bool has_results)
    {
        push_cursor_coord();
        set_cursor_pos(m_picker);
        write(" ");

        if (has_results) {
            m_picker.m_x = 0;
            m_picker.m_y = m_max_y - 2;

            set_cursor_pos(m_picker);
            write<red>(">");
        }

        pop_cursor_coord();

        return *this;
    }

    /**
     * Moves picker in provided direction. Results size is used to limit movement scope.
     */
    template<Direction d>
    Console& move_picker(u64 results_size)
    {
        push_cursor_coord();
        set_cursor_pos(m_picker);
        write(" ");

        if constexpr (d == Direction::up) {
            u32 max1 = std::max(m_picker.m_y - 1U, m_min_y);
            u32 max2 =
                results_size > m_max_y - 1 ? m_min_y : m_max_y - 1 - static_cast<u32>(results_size);

            m_picker.m_y = std::max(max1, max2);
        }
        else if constexpr (d == Direction::down)
            m_picker.m_y = std::min(m_picker.m_y + 1U, m_max_y - 2);
        else
            static_assert("Invalid direction.");

        set_cursor_pos(m_picker);
        write<red>(">");
        pop_cursor_coord();

        return *this;
    }

    /**
     * Copies result string from picker position into the clipboard.
     */
    template<CopyOpt copy_opt>
    Console& copy_result_to_clipboard(const Files::Matches& results)
    {
        assert(!results.empty());
        u32 first = m_max_y - 2;
        sz idx = first - m_picker.m_y;

        assert(idx >= 0 && idx < results.size());
        idx = std::clamp(idx, sz(0), results.size());

        if constexpr (copy_opt == CopyOpt::file_name)
            os::copy_to_clipboard<true>(std::string(results[idx].m_file->name()));
        else if constexpr (copy_opt == CopyOpt::file_path)
            os::copy_to_clipboard<true>(std::string(results[idx].m_file->path()));
        else if constexpr (copy_opt == CopyOpt::full)
            os::copy_to_clipboard<true>(results[idx].m_file->full_path());
        else if constexpr (copy_opt == CopyOpt::full_quoted)
            os::copy_to_clipboard<true>("\"" + results[idx].m_file->full_path() + "\"");
        else
            static_assert(false, "Invalid copy opt.");

        return *this;
    }

    /**
     * Draws search results on the screen.
     * We are always on a bottom line when this funcion is called.
     */
    Console& draw_search_results(const Files::Matches& matches)
    {
        move_cursor<up>(2).move_cursor_to<edge_left>().move_cursor<right>();

        auto it = matches.data().begin();
        const auto it_end = matches.data().end();

        while (y() >= min_y()) {
            if (it != it_end) {
                draw_single_search_result(it);
                ++it;
            }

            clear_rest_of_line();

            if (y() == min_y())
                break;

            move_cursor<up>().move_cursor_to<edge_left>().move_cursor<right>();
        }

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
    u32 m_min_x = 1U;
    u32 m_min_y = 1U;
    u32 m_max_x;
    u32 m_max_y;
    std::array<Coord, coord_stack_size> m_coord_stack{};
    u32 m_stack_size = 0;
    Coord m_picker; // ">" - file picker.
    Color m_color = term_default;
    std::string m_stream; // need to cache cout, because of horrible windows terminal performance.
};

#endif // CONSOLE_H
