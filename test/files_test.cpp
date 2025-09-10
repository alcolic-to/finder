#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>

#include "files.h"
#include "os.h"
#include "util.h"

// NOLINTBEGIN

TEST(files_test, sanity_test_1)
{
    Files files;

    const std::string file_name = "my_file_1";
    std::string file_path =
#if defined _WIN32
        R"(C:\User\win_user_1)";
#elif defined __linux__
        R"(/User/win_user_1)";
#endif

    const std::string file = file_path + os::path_sep + file_name;

    files.insert(file);
    ASSERT_TRUE(!files.insert(file));

    auto r = files.search("my_file_1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    r = files.search("m");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    r = files.search("my");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    r = files.search("file");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    r = files.search("_");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    r = files.search("1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->name() == file_name);
    ASSERT_TRUE(r[0]->path() == file_path);

    files.erase(file);

    r = files.search("");
    ASSERT_TRUE(r.size() == 0);

    r = files.search("my_file_1");
    ASSERT_TRUE(r.size() == 0);
}

TEST(files_test, sanity_test_2)
{
    Files files;

    std::string file_name = "attach.cpp";

    std::string file_path_1 =

#if defined _WIN32
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\linux_and_mac)";
#elif defined __linux__
        R"(C:/Users/win_user_1/.vscode/extensions/ms-python.debugpy-2025.6.0-win32-x64/bundled/libs/debugpy/_vendored/pydevd/pydevd_attach_to_process/linux_and_mac)";
#endif

    std::string file_path_2 =
#if defined _WIN32
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\windows)";
#elif defined __linux__
        R"(C:/Users/win_user_1/.vscode/extensions/ms-python.debugpy-2025.6.0-win32-x64/bundled/libs/debugpy/_vendored/pydevd/pydevd_attach_to_process/windows)";
#endif

    const std::string file_1 = file_path_1 + os::path_sep + file_name;
    const std::string file_2 = file_path_2 + os::path_sep + file_name;

    files.insert(file_1);
    files.insert(file_2);

    auto r = files.search("attach.cpp");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path_1 || r[1]->path() == file_path_1);
    ASSERT_TRUE(r[0]->path() == file_path_2 || r[1]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name && r[1]->name() == file_name);

    r = files.search("attach");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path_1 || r[1]->path() == file_path_1);
    ASSERT_TRUE(r[0]->path() == file_path_2 || r[1]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name && r[1]->name() == file_name);

    r = files.search("cpp");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path_1 || r[1]->path() == file_path_1);
    ASSERT_TRUE(r[0]->path() == file_path_2 || r[1]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name && r[1]->name() == file_name);

    r = files.search(".");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path_1 || r[1]->path() == file_path_1);
    ASSERT_TRUE(r[0]->path() == file_path_2 || r[1]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name && r[1]->name() == file_name);

    files.erase(file_1);

    r = files.search("attach.cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name);

    r = files.search("cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->path() == file_path_2);
    ASSERT_TRUE(r[0]->name() == file_name);
}

TEST(files_test, sanity_test_3)
{
    Files files;

    std::string file_name_1 = "attach_1.cpp";
    std::string file_name_2 = "attach_2.cpp";

    std::string file_path =
#if defined _WIN32
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\linux_and_mac)";
#elif defined __linux__
        R"(C:/Users/win_user_1/.vscode/extensions/ms-python.debugpy-2025.6.0-win32-x64/bundled/libs/debugpy/_vendored/pydevd/pydevd_attach_to_process/linux_and_mac)";
#endif

    const std::string file_1 = file_path + os::path_sep + file_name_1;
    const std::string file_2 = file_path + os::path_sep + file_name_2;

    files.insert(file_1);
    files.insert(file_2);

    auto r = files.search("attach_");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path && r[1]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_1 || r[1]->name() == file_name_1);
    ASSERT_TRUE(r[0]->name() == file_name_2 || r[1]->name() == file_name_2);

    r = files.search("attach");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path && r[1]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_1 || r[1]->name() == file_name_1);
    ASSERT_TRUE(r[0]->name() == file_name_2 || r[1]->name() == file_name_2);

    r = files.search("cpp");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path && r[1]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_1 || r[1]->name() == file_name_1);
    ASSERT_TRUE(r[0]->name() == file_name_2 || r[1]->name() == file_name_2);

    r = files.search(".");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->path() == file_path && r[1]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_1 || r[1]->name() == file_name_1);
    ASSERT_TRUE(r[0]->name() == file_name_2 || r[1]->name() == file_name_2);

    files.erase(file_1);

    r = files.search("attach");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_2);

    r = files.search("cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->path() == file_path);
    ASSERT_TRUE(r[0]->name() == file_name_2);
}

TEST(files_test, file_system_paths)
{
    Files files;

    std::ifstream in_file_stream{std::string(PROJECT_ROOT) +
                                 "/test/input_files/windows_paths_vscode.txt"};
    ASSERT_TRUE(in_file_stream.is_open());

    size_t cpp_files_count = 0;

    for (std::string file_path; std::getline(in_file_stream, file_path);) {
        auto path = std::filesystem::path(file_path);

        auto file_name = path.filename().string();
        if (file_name.find("cpp") != std::string::npos)
            ++cpp_files_count;

        files.insert(path);
    }

    ASSERT_TRUE(files.search("cpp").size() == cpp_files_count);
}

void test_fs_search(const std::string& file_name)
{
    Files files;

    std::ifstream in_file_stream{std::string(PROJECT_ROOT) + "/test/input_files/" + file_name};
    ASSERT_TRUE(in_file_stream.is_open());

    std::vector<std::string> paths;

    {
        Stopwatch<true, milliseconds> s{"Scanning"};
        for (std::string file_path; std::getline(in_file_stream, file_path);) {
            paths.emplace_back(std::move(file_path));
            files.insert(paths.back());
        }
    }

    constexpr sz MB = 1024 * 1024;

    std::cout << std::format("Files paths size:    {}MB\n", files.file_paths_size() / MB);
    std::cout << std::format("Files size:          {}MB\n", files.files_size() / MB);

    // std::cout << "Paths count: " << paths.size() << "\n";

    // std::string str_for_match;
    // while (true) {
    // std::cout << ": ";
    // std::cin >> str_for_match;

    // Stopwatch<true, microseconds> s{"Search + print"};
    // auto r = files.search(str_for_match);
    // auto elapsed = s.elapsed();

    // for (auto* finfo : r)
    // std::cout << finfo->path() << "\n";

    // std::cout << "Search time: " << duration_cast<microseconds>(elapsed) << "\n";
    // }
}

TEST(files_test, fs_search)
{
    // GTEST_SKIP() << "Skipping filesystem path search test, because it is for manual testing
    // only.";

    std::vector<std::string> file_names{
        "windows_paths_vscode.txt",
        "linux_paths.txt",
        "windows_paths.txt",
    };

    for (const auto& file_name : file_names)
        test_fs_search(file_name);
}

// NOLINTEND
