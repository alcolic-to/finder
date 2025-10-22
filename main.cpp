#include <cctype>
#include <chrono>
#include <cstddef>
#include <exception>
#include <memory>
#include <thread>

#include "ast.h"
#include "cli11/CLI11.hpp"
#include "console.h"
#include "cos-cooperative-scheduling/async.h"
#include "cos-cooperative-scheduling/options.h"
#include "cos-cooperative-scheduling/scheduler.h"
#include "cos-cooperative-scheduling/ums.h"
#include "files.h"
#include "finder.h"
#include "os.h"
#include "small_string.h"
#include "util.h"

// NOLINTBEGIN(misc-use-anonymous-namespace, readability-implicit-bool-conversion)

static bool level_down(std::string& query)
{
    if (query.empty())
        return false;

    query.pop_back();
    while (!query.empty() && query.back() != os::path_sep)
        query.pop_back();

    return true;
}

static bool level_up(std::string& query, const Files::Match& match)
{
    const SmallString& name = match.m_file->name();
    const std::string_view& path = match.m_file->path();
    std::string full_path = match.m_file->full_path();

    sz slash_pos = query.find_last_of(os::path_sep);

    std::string query_name{slash_pos != std::string::npos ? query.substr(slash_pos + 1) : query};
    std::string query_path{slash_pos != std::string::npos ? query.substr(0, slash_pos + 1) : ""};

    if (path == query_path) {
        if (query_name == name)
            return false;

        query = query_path + name.str();
        return true;
    }

    for (auto it = full_path.begin() + query_path.size(); it != full_path.end(); ++it) {
        query_path.append(1, *it);
        if (*it == os::path_sep)
            break;
    }

    query = query_path + query_name;
    return true;
}

static bool scan_input(Console& console, std::string& query, const Files::Matches& results)
{
    i32 input_ch = 0;
    while (true) {
        console >> input_ch;

        if (os::is_term(input_ch))
            return false; // Terminate.
        else if (os::is_esc(input_ch))
            ; // -> Ignore escape.
        else if (os::is_ctrl_j(input_ch)) {
            if (!results.empty()) {
                console.move_picker<down>(results);
                console.flush();
            }
        }
        else if (os::is_ctrl_k(input_ch)) {
            if (!results.empty()) {
                console.move_picker<up>(results);
                console.flush();
            }
        }
        else if (os::is_ctrl_h(input_ch)) {
            if (level_down(query))
                break;
        }
        else if (os::is_ctrl_l(input_ch)) {
            if (results.empty())
                continue;

            if (level_up(query, console.pick_result(results)))
                break;
        }
        else if (os::is_ctrl_f(input_ch)) {
            if (!results.empty())
                console.copy_result_to_clipboard<CopyOpt::file_name>(results);
        }
        else if (os::is_ctrl_p(input_ch)) {
            if (!results.empty())
                console.copy_result_to_clipboard<CopyOpt::file_path>(results);
        }
        else if (os::is_ctrl_y(input_ch)) {
            if (!results.empty())
                console.copy_result_to_clipboard<CopyOpt::full>(results);
        }
        else if (os::is_ctrl_u(input_ch)) {
            if (!results.empty())
                console.copy_result_to_clipboard<CopyOpt::full_quoted>(results);
        }
        else if (os::is_ctrl_d(input_ch)) {
            query.clear();
            break;
        }
        else if (os::is_backspace(input_ch)) {
            if (!query.empty()) {
                query.pop_back();
                break;
            }
        }
        else if (std::isprint(static_cast<u8>(input_ch))) {
            query += static_cast<char>(input_ch);
            break;
        }
    }

    return true;
}

int finder_main(const Options& opt) // NOLINT
{
    Finder finder{opt};

    /* Query related info. */
    std::string query;
    Files::Matches results;
    milliseconds time = 0ms;
    sz objects_count = 0;

    /* Console related. */
    Console console;

    /* Tasks related. */
    u32 cpus_count = ums::schedulers->cpus_count();
    u32 workers_count = ums::schedulers->workers_count();
    u32 task_id = 0;
    u32 tasks_count = opt.tasks_count();
    std::vector<ums::Task<Files::Matches>> tasks;
    tasks.reserve(tasks_count);

    while (true) {
        results.clear();
        tasks.clear();

        {
            Stopwatch<false, milliseconds> sw;

            for (task_id = 0; task_id < tasks_count; ++task_id) {
                tasks.emplace_back(ums::async([&, tasks_count, task_id] {
                    return finder.find_files_partial(query, tasks_count, task_id);
                }));
            }

            for (auto& task : tasks) {
                const Files::Matches matches = task->get();
                results.insert(matches);
            }

            time = sw.elapsed_units();
            objects_count = results.objects_count();
        }

        console.move_cursor_to<edge_bottom>().move_cursor_to<edge_left>().move_cursor<right>();

        console.write("Search: {}", query);
        console.clear_rest_of_line();
        console.push_cursor_coord();

        console.push_cursor_coord();
        console.move_cursor_to<edge_right>().move_cursor<left>(70); // NOLINT
        console.write("cpus: {}, workers: {}, tasks: {}, objects: {}, search time: {}", cpus_count,
                      workers_count, tasks_count, objects_count, time);
        console.pop_cursor_coord();

        console.print_search_results(results);
        console.pop_cursor_coord();
        // console.draw_symbol_search_results(finder.find_symbols(query));

        console.init_picker(results);
        console.flush();

        if (!scan_input(console, query, results))
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

// NOLINTEND(misc-use-anonymous-namespace, readability-implicit-bool-conversion)
