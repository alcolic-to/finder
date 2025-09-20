#include <cctype>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>

#include "ast.h"
#include "async.h"
#include "console.h"
#include "finder.h"
#include "mutex.h"
#include "options.h"
#include "os.h"
#include "scheduler.h"
#include "symbol_finder.h"
#include "ums.h"
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

int finder_main(int argc, char* argv[])
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

    /* Query related info. */
    std::string query{""};
    std::vector<const FileInfo*> results;
    milliseconds time = 0ms;
    sz objects_count = 0;

    /* Console related. */
    Console console;

    /* Tasks related. */
    u32 workers_count = ums::schedulers->workers_count();
    u32 worker_id = 0;
    std::vector<std::shared_ptr<ums::Task>> tasks;
    tasks.reserve(workers_count);
    ums::Mutex mtx;

    while (true) {
        results.clear();

        {
            Stopwatch<false, milliseconds> sw;

            for (worker_id = 0; worker_id < workers_count; ++worker_id) {
                /*
                 * Single worker search task.
                 */
                auto search_task = [&, worker_id] {
                    std::vector<const FileInfo*> partial =
                        finder.find_files_partial(query, workers_count, worker_id);

                    std::scoped_lock<ums::Mutex> lock{mtx};
                    results.insert(results.end(), partial.begin(), partial.end());
                };

                tasks.emplace_back(ums::async(search_task));
            }

            for (auto& task : tasks)
                task->wait();

            time = sw.elapsed_units();
            objects_count = results.size();
        }

        console.draw_search_results(results);
        // console.draw_symbol_search_results(finder.find_symbols(query));

        console.move_cursor_to<edge_bottom>().move_cursor_to<edge_left>();

        console.write("Search: {}", query);
        console.clear_rest_of_line();

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

int main(int argc, char* argv[])
{
    u32 cpus_count = std::clamp(std::thread::hardware_concurrency(), u32(1), u32(64));

    auto sch = ums::Options::Schedulers_count{cpus_count};
    auto workers = ums::Options::Workers_per_scheduler{4};

    ums::init_ums([&] { finder_main(argc, argv); }, ums::Options{sch, workers});
}

// NOLINTEND
