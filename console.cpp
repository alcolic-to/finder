/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "console.hpp"

#include <cstddef>
#include <iostream>
#include <string>

#include "os.hpp"

/**
 * We will map cursor coordinates as a windows console:
 * Defines the coordinates of a character cell in a console screen buffer. The origin of the
 * coordinate system (0, 0) is at the top, left cell of the buffer.
 * X - the horizontal coordinate or column value.
 * Y - the vertical coordinate or row value.
 */
Console::Console()
    : m_in_handle{os::init_console_in_handle()}
    , m_out_handle{os::init_console_out_handle()}
{
    os::Coordinates coord = os::console_window_size(m_out_handle);
    m_max_x = std::max(i16(1), coord.x);
    m_max_y = std::max(i16(1), coord.y);
    m_picker.m_x = m_min_x;
    m_picker.m_y = m_max_y < 2 ? 1 : m_max_y - 2;
    m_stream.reserve(usize(m_max_x) * m_max_y);

    clear();
    flush();
}

Console::~Console()
{
    clear();
    write<term_default>(""); // Reset color.
    flush();
    os::close_console(m_in_handle, m_out_handle);
}

void Console::resize(os::Coordinates coord)
{
    clear();
    flush();

    m_max_x = std::max(i16(1), coord.x);
    m_max_y = std::max(i16(1), coord.y);
    m_picker.m_x = m_min_x;
    m_picker.m_y = m_max_y < 2 ? 1 : m_max_y - 2;
}

Console& Console::operator>>(os::ConsoleInput& input)
{
    os::console_scan(m_in_handle, input);
    return *this;
}

Console& Console::clear()
{
    command("2J");
    command("H");
    set_cursor_pos(1, 1);
    return *this;
}

Console& Console::flush()
{
    std::cout << m_stream;
    std::cout.flush();
    m_stream.clear();

    return *this;
}

Console& Console::clear_rest_of_line()
{
    command("K");
    return *this;
};

Console& Console::apply_cursor_pos()
{
    command("{};{}H", m_y, m_x);
    return *this;
}

Console& Console::set_cursor_pos(Coord coord)
{
    return set_cursor_pos(coord.m_x, coord.m_y);
}

Console& Console::set_cursor_pos(u32 x, u32 y)
{
    m_x = x, m_y = y;
    apply_cursor_pos();

    return *this;
}

Console& Console::push_cursor_coord()
{
    if (m_stack_size > coord_stack_size)
        throw std::runtime_error{"Stack is full."};

    m_coord_stack[m_stack_size++] = coord(); // NOLINT
    return *this;
}

Console& Console::pop_cursor_coord()
{
    if (m_stack_size <= 0)
        throw std::runtime_error{"Stack is empty."};

    Coord c = m_coord_stack[--m_stack_size]; // NOLINT
    m_x = c.m_x, m_y = c.m_y;

    apply_cursor_pos();
    return *this;
}

[[nodiscard]] const Files::Match& Console::pick_result(const Files::Matches& results) const
{
    usize idx = m_max_y - 2 - m_picker.m_y;
    if (idx > results.size())
        throw std::runtime_error{"Invalid index for pick result."};

    return results[m_max_y - 2 - m_picker.m_y];
}

/**
 * Prints single search result on a current line.
 * If picked is provided, it means that we have a picker on that line, and we must color
 * backround with different color.
 * Also, if user have pinned search path, we will not print it.
 * Current limit for matched letters in a single word is 63.
 */
template<bool picked>
Console& Console::print_single_search_result(const Files::Match& match, const Query& query)
{
    const auto& bs = match.m_match_bs;
    const FileInfo* file = match.m_file;

    std::string print = file->full_path();
    for (usize i = query.m_pinned.size(); i < print.size(); ++i) {
        if (i < bs.size() && bs.test(i))
            if constexpr (picked)
                write<green, gray>(print[i]);
            else
                write<green>(print[i]);
        else if constexpr (picked)
            write<term_default, gray>(print[i]);
        else
            write(print[i]);
    }

    set_color<term_default, term_default>();
    return *this;
}

/**
 * Prints search results on the screen.
 * We are always on a bottom line when this funcion is called.
 */
Console& Console::print_search_results(const Files::Matches& matches, const Query& query)
{
    move_cursor<up>(2).move_cursor_to<edge_left>().move_cursor<right>();

    auto it = matches.data().begin();
    const auto it_end = matches.data().end();

    while (y() >= min_y()) {
        if (it != it_end) {
            print_single_search_result(*it, query);
            ++it;
        }

        clear_rest_of_line();

        if (y() == min_y())
            break;

        move_cursor<up>().move_cursor_to<edge_left>().move_cursor<right>();
    }

    return *this;
}

/**
 * Initializes picker.
 * It first deletes current picker from console, and, if search results exist, shows picker on
 * initial position.
 */
Console& Console::init_picker(const Files::Matches& results, const Query& query)
{
    push_cursor_coord();
    set_cursor_pos(m_picker);
    write(" ");

    if (!results.empty()) {
        u32 offset = m_max_y - 1 >= results.size() ? m_max_y - 1 - results.size() : m_min_y;
        m_picker.m_y = std::max(m_picker.m_y, offset);

        set_cursor_pos(m_picker);
        write<red>(">");

        print_single_search_result<true>(results[m_max_y - 2 - m_picker.m_y], query);
    }

    pop_cursor_coord();

    return *this;
}

/**
 * Moves picker in provided direction. Results size is used to limit movement scope.
 */
template<Direction d>
Console& Console::move_picker(const Files::Matches& results, const Query& query)
{
    push_cursor_coord();

    set_cursor_pos(m_picker);
    write(" ");
    print_single_search_result<false>(results[m_max_y - 2 - m_picker.m_y], query);

    if constexpr (d == Direction::up) {
        u32 max1 = std::max(m_picker.m_y - 1U, m_min_y);
        u32 max2 =
            results.size() > m_max_y - 1 ? m_min_y : m_max_y - 1 - static_cast<u32>(results.size());

        m_picker.m_y = std::max(max1, max2);
    }
    else if constexpr (d == Direction::down)
        m_picker.m_y = std::min(m_picker.m_y + 1U, m_max_y - 2);
    else
        static_assert("Invalid direction.");

    set_cursor_pos(m_picker);
    write<red>(">");
    print_single_search_result<true>(results[m_max_y - 2 - m_picker.m_y], query);

    pop_cursor_coord();

    return *this;
}

/**
 * Copies result string from picker position into the clipboard.
 */
template<CopyOpt copy_opt>
Console& Console::copy_result_to_clipboard(const Files::Matches& results)
{
    assert(!results.empty());

    u32 first = m_max_y - 2;
    usize idx = first - m_picker.m_y;

    assert(idx >= 0 && idx < results.size());
    idx = std::clamp(idx, usize(0), results.size());

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

Console& Console::draw_symbol_search_results(const Symbol* symbol)
{
    move_cursor_to<edge_top>();
    move_cursor_to<edge_left>();

    if (symbol != nullptr) {
        for (const auto& symref : symbol->refs()) {
            for (const auto& line : symref.lines()) {
                clear_rest_of_line();

                write("{}\\{} {}: {}\n", symref.file()->path(), symref.file()->name().c_str(),
                      line.number(), std::string(line.preview()));

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

void Console::render_main(const Query& query, u32 cpus_count, u32 workers_count, u32 tasks_count,
                          u32 objects_count, const Files::Matches& results,
                          std::chrono::duration<long long, std::ratio<1, 1000>> time)
{
    if (m_max_x < min_x_required || m_max_y < min_y_required) {
        write("Window too small.");
        flush();
        return;
    }

    move_cursor_to<edge_bottom>().move_cursor_to<edge_left>();

    write("{}: {}", query.m_pinned, query.m_query);
    clear_rest_of_line();

    push_cursor_coord();

    push_cursor_coord();
    move_cursor_to<edge_right>().move_cursor<left>(70); // NOLINT
    write("cpus: {}, workers: {}, tasks: {}, objects: {}, search time: {}", cpus_count,
          workers_count, tasks_count, objects_count, time);
    pop_cursor_coord();

    print_search_results(results, query);
    pop_cursor_coord();

    init_picker(results, query);

    flush();
}

template Console& Console::move_picker<Direction::up>(const Files::Matches&, const Query&);
template Console& Console::move_picker<Direction::down>(const Files::Matches&, const Query&);
template Console& Console::copy_result_to_clipboard<CopyOpt::file_name>(const Files::Matches&);
template Console& Console::copy_result_to_clipboard<CopyOpt::file_path>(const Files::Matches&);
template Console& Console::copy_result_to_clipboard<CopyOpt::full>(const Files::Matches&);
template Console& Console::copy_result_to_clipboard<CopyOpt::full_quoted>(const Files::Matches&);
