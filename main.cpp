#include <cctype>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "CLI11.hpp"
#include "ast.h"
#include "async.h"
#include "console.h"
#include "finder.h"
#include "mutex.h"
#include "options.h"
#include "os.h"
#include "scheduler.h"
#include "ums.h"
#include "util.h"

// NOLINTBEGIN

bool scan_input(Console& console, std::string& query)
{
    i32 input_ch;
    while (true) {
        console >> input_ch;

        if (os::is_term(input_ch))
            return false;

        if (os::is_esc(input_ch))
            continue; // -> Ignore escape.

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

int finder_main(const Options& opt)
{
    Finder finder{opt};
    // Symbol_finder finder{root, Options{ss.str()}};

    /* Query related info. */
    std::string query{""};
    std::vector<const FileInfo*> results;
    milliseconds time = 0ms;
    sz objects_count = 0;

    /* Console related. */
    Console console;

    /* Tasks related. */
    u32 cpus_count = ums::schedulers->cpus_count();
    u32 workers_count = ums::schedulers->workers_count();
    u32 worker_id = 0;
    u32 tasks_count = opt.tasks_count();
    std::vector<std::shared_ptr<ums::Task>> tasks;
    tasks.reserve(tasks_count);
    ums::Mutex mtx;

    while (true) {
        results.clear();

        {
            Stopwatch<false, milliseconds> sw;

            for (worker_id = 0; worker_id < tasks_count; ++worker_id) {
                /*
                 * Single worker search task.
                 */
                auto search_task = [&, worker_id] {
                    std::vector<const FileInfo*> partial =
                        finder.find_files_partial(query, tasks_count, worker_id);

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
        console.move_cursor_to<edge_right>().move_cursor<left>(70);
        console.write("cpus: {}, workers: {}, tasks: {}, objects: {}, search time: {}", cpus_count,
                      workers_count, tasks_count, objects_count, time);
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
    CLI::App app{"Finder application that searches files and symbols."};
    argv = app.ensure_utf8(argv);

    std::string root = os::root_dir();
    std::vector<std::string> ignore_list;
    std::vector<std::string> include_list;
    bool files = true;
    bool symbols = false;
    bool stats_only = false;
    bool verbose = false;
    u32 wps = 2;
    u32 cpus = std::thread::hardware_concurrency();
    u32 tasks_count = cpus;

    app.add_option("-r,--root", root,
                   "Root directory for files/symbols. Default is OS root directory.");
    app.add_option("-i,--ignore", ignore_list,
                   "Ignores provided paths. Paths should be separated by space.");
    app.add_option(
        "-n,--include", include_list,
        "Includes provided paths even if they are ignored. Paths should be separated by space.");
    app.add_flag("-f,--files", files, "Files search. Default is true.");
    app.add_flag("-s,--symbols", symbols, "Symbols search. Default is false.");
    app.add_flag("-o,--stat-only", stats_only, "Prints stats and quit. Default is false.");
    app.add_flag("-v,--verbose", verbose, "Enables verbose output. Default is false.");

    app.add_option("-w,--workers", wps, "Number of workers per scheduler.");
    app.add_option("-c,--cpus", cpus, "Number of CPUs to be used. Default is all available CPUs.");
    app.add_option("-t,--tasks-count", tasks_count,
                   "Number of search tasks. Default is number of CPUs.");

    CLI11_PARSE(app, argc, argv);

    ums::Options ums_opt{ums::Options::Schedulers_count{cpus},
                         ums::Options::Workers_per_scheduler{wps}};
    Options finder_opt{root,    ignore_list, include_list, files,
                       symbols, stats_only,  verbose,      tasks_count};

    ums::init_ums([&] { finder_main(finder_opt); }, ums_opt);
}

// NOLINTEND
