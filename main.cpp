#include <cctype>
#include <cstdint>
#include <sstream>

#include "console.h"
#include "symbol_finder.h"

// NOLINTBEGIN

int main()
{
    Console console;

    std::string input;
    std::string root;
    std::string options;
    std::string regex;

    console.clear();
    console << "Options: <root_dir> <-fs>\n: ";
    console.getline(input);

    std::stringstream ss{input};
    ss >> root, ss >> options;

    uint32_t u32_opt = 0;
    if (options.find('f') != std::string::npos)
        u32_opt |= Options::files;
    if (options.find('s') != std::string::npos)
        u32_opt |= Options::symbols;

    Symbol_finder finder{root, Options{u32_opt}};

    // Show all files.
    //
    console.clear();
    auto files{finder.find_files("")};
    console.draw_search_results(files);

    Cursor& cursor = console.cursor();
    std::string query;

    while (true) {
        cursor.move_to<e_bottom>();
        cursor.move_to<e_left>();

        console.fill_line(' ');
        console << "Search: " << query;

        for (int32_t input_ch;;) {
            console >> input_ch;

            if (input_ch == 27) // ESC
                return 0;

            if (input_ch == 8 && !query.empty()) { // backspace
                query.pop_back();
                break;
            }
            else if (std::isprint(input_ch)) {
                query += input_ch;
                break;
            }
        }

        auto files{finder.find_files(query)};
        console.draw_search_results(files);
    }

    return 0;
}

// NOLINTEND
