#include <cctype>
#include <chrono>
#include <cstdint>
#include <sstream>

#include "ast.h"
#include "console.h"
#include "finder.h"
#include "os.h"
#include "symbol_finder.h"
#include "util.h"

// NOLINTBEGIN

bool scan_input(Console& console, std::string& query)
{
    i32 input_ch;
    while (true) {
        console >> input_ch;

        if (os::is_esc(input_ch))
            return false;

        if (os::is_backspace(input_ch)) {
            if (query.empty())
                continue;

            query.pop_back();
            break;
        }

        u8 prnt = static_cast<u8>(input_ch);
        if (std::isprint(prnt)) {
            query += prnt;
            break;
        }
    }

    return true;
}

int main()
{
    std::string input;
    std::string root;

    std::cout << "Options: <root_dir> [-fsev] [--ignore=<path1,path2 ... >], "
                 "[--include=<path1,path2 ... >]\n: ";

    std::getline(std::cin, input);

    std::stringstream ss{input};
    ss >> root;

    Finder finder{root, Options{ss.str()}};
    // Symbol_finder finder{root, Options{ss.str()}};

    std::string query{""};
    std::vector<const File_info*> results;
    milliseconds time = 0ms;
    sz objects_count = 0;

    Console console;

    while (true) {
        {
            Stopwatch<false, milliseconds> sw;
            results = finder.find_files(query);

            time = sw.elapsed_units();
            objects_count = results.size();
        }

        console.draw_search_results(results);
        // console.draw_symbol_search_results(finder.find_symbols(query));

        console.move_cursor_to<edge_bottom>().move_cursor_to<edge_left>();

        console.clear_line();
        console.write("Search: {}", query);

        console.push_coord();
        console.move_cursor_to<edge_right>().move_cursor<left>(40);
        console.write("objects: {}, search time: {}", objects_count, time);
        console.pop_coord();
        console.flush();

        if (!scan_input(console, query))
            break;
    }

    console.write("\n");
    return 0;
}

// NOLINTEND
