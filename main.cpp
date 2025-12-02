#include <cctype>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <variant>

#include "cli11/CLI11.hpp"
#include "console.h"
#include "files.h"
#include "finder.h"
#include "os.h"
#include "query.h"
#include "ums/async.hpp"
#include "ums/options.hpp"
#include "ums/scheduler.hpp"
#include "ums/ums.hpp"
#include "util.h"

// NOLINTBEGIN(misc-use-anonymous-namespace, readability-implicit-bool-conversion,
// readability-function-cognitive-complexity)

/**
 * Moves pinned query path one level down.
 * If pinned path is on the "seconds" level (/usr/), we will jump directly to the empty path,
 * because there is no point in pinning root only (/).
 */
static bool level_down(Query& query)
{
    std::string& path = query.m_pinned;

    if (path.empty())
        return false;

    path.pop_back();
    while (!path.empty() && path.back() != os::path_sep)
        path.pop_back();

    if (path == os::path_sep_str)
        path.clear();

    return true;
}

/**
 * Moves pinned query path one level up.
 * If pinned path is empty, we will jump directly to the "second" level, skipping root, because
 * there is no point in pinning root only (/).
 * We will try to go level up "smartly", meaning that we will remove part of path from user query
 * that will be pinned.
 */
static bool level_up(Query& query, const Files::Match& match)
{
    std::string& q_query = query.m_query;
    std::string& q_path = query.m_pinned;

    sz slash_pos = q_query.find_last_of(os::path_sep);
    std::string query_name{slash_pos != std::string::npos ? q_query.substr(slash_pos + 1) :
                                                            q_query};
    std::string query_path{slash_pos != std::string::npos ? q_query.substr(0, slash_pos + 1) : ""};

    const std::string full_path = match.m_file->full_path().substr();

    if (q_path == match.m_file->path())
        query_name.clear();

    for (auto it = full_path.begin() + q_path.size(); it != full_path.end(); ++it) {
        q_path.append(1, *it);
        if (*it == os::path_sep && q_path != os::path_sep_str)
            break;
    }

    if (!q_path.ends_with(os::path_sep))
        q_path.append(1, os::path_sep);

    if (query_path.starts_with(os::path_sep))
        query_path = query_path.substr(1);

    slash_pos = query_path.find_first_of(os::path_sep);
    if (slash_pos != std::string::npos)
        query_path = query_path.substr(slash_pos + 1);

    q_query = query_path + query_name;
    return true;
}

/**
 * Pins path from picker position and removes path from name.
 */
static void pin_path(Query& query, const Files::Match& match)
{
    query.m_pinned = match.m_file->full_path();
    if (!query.m_pinned.ends_with(os::path_sep))
        query.m_pinned.append(1, os::path_sep);

    query.m_query.clear();
}

enum class Command { normal, consol_resize, exit }; // NOLINT

static Command handle_command(Console& console, Query& query, const Files::Matches& results)
{
    os::ConsoleInput input;
    i32 input_ch = 0;

    while (true) {
        console >> input;

        if (std::holds_alternative<os::Coordinates>(input)) {
            console.resize(std::get<os::Coordinates>(input));
            return Command::consol_resize;
        }

        input_ch = std::get<i32>(input);

        if (os::is_term(input_ch) || os::is_ctrl_q(input_ch))
            return Command::exit; // Terminate.
        else if (os::is_esc(input_ch))
            ; // -> Ignore escape.
        else if (os::is_ctrl_j(input_ch)) {
            if (!results.empty()) {
                console.move_picker<down>(results, query);
                console.flush();
            }
        }
        else if (os::is_ctrl_k(input_ch)) {
            if (!results.empty()) {
                console.move_picker<up>(results, query);
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
        else if (os::is_ctrl_i(input_ch)) {
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
            query.m_query.clear();
            break;
        }
        else if (os::is_ctrl_g(input_ch)) {
            query.m_pinned.clear();
            break;
        }
        else if (os::is_ctrl_p(input_ch)) {
            if (!results.empty()) {
                pin_path(query, console.pick_result(results));
                break;
            }
        }
        else if (os::is_backspace(input_ch)) {
            if (!query.m_query.empty()) {
                query.m_query.pop_back();
                break;
            }
        }
        else if (std::isprint(static_cast<u8>(input_ch))) {
            query.m_query += static_cast<char>(input_ch);
            break;
        }
    }

    return Command::normal;
}

int finder_main(const Options& opt) // NOLINT
{
    Finder finder{opt};

    /* Search results related. */
    Query query;
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
                    return finder.find_files_partial(query.full(), tasks_count, task_id);
                }));
            }

            for (auto& task : tasks) {
                const Files::Matches matches = task->get();
                results.insert(matches);
            }

            time = sw.elapsed_units();
            objects_count = results.objects_count();
        }

        console.render_main(query, cpus_count, workers_count, tasks_count, objects_count, results,
                            time);

        Command c;
        while ((c = handle_command(console, query, results)) != Command::normal) {
            switch (c) {
            case Command::consol_resize:
                console.render_main(query, cpus_count, workers_count, tasks_count, objects_count,
                                    results, time);
                break; // breaks from switch;
            case Command::exit:
                return 0;
            default:
                assert(!"Invalid scan result.");
                unreachable();
            }
        }
    }
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

// NOLINTEND(misc-use-anonymous-namespace, readability-implicit-bool-conversion,
// readability-function-cognitive-complexity)
