#include <cctype>
#include <cstdint>
#include <sstream>

#include "console.h"
#include "os.h"
#include "symbol_finder.h"

// NOLINTBEGIN

bool scan_input(Console& console, std::string& query)
{
    int32_t input_ch;
    while (true) {
        console >> input_ch;

        if (os::is_esc(input_ch)) // ESC
            return false;

        if (os::is_backspace(input_ch)) { // backspace
            if (query.empty())
                continue;

            query.pop_back();
            break;
        }

        uint8_t prnt = static_cast<uint8_t>(input_ch);
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
    std::string options;

    std::cout << "Options: <root_dir> <-fse>\n: ";
    std::getline(std::cin, input);

    std::stringstream ss{input};
    ss >> root, ss >> options;

    Symbol_finder finder{root, Options{options}};

    // Show all files/symbols.
    //
    Console console;
    console.clear();
    console.draw_search_results(finder.find_files(""));
    // console.draw_symbol_search_results(finder.find_symbols(""));

    Cursor& cursor = console.cursor();
    std::string query;

    while (true) {
        cursor.move_to<e_bottom>();
        cursor.move_to<e_left>();

        console.fill_line(' ');
        console << "Search: " << query;

        if (!scan_input(console, query)) {
            console << "\n";
            return 0;
        }

        console.draw_search_results(finder.find_files(query));
        // console.draw_symbol_search_results(finder.find_symbols(query));
    }

    return 0;
}

// NOLINTEND
