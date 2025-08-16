#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>

#include "fs_trie.h"
#include "util.h"

// NOLINTBEGIN

TEST(fs_trie_tests, sanity_test_1)
{
    FS_trie trie;

    const std::string file_name = "my_file_1";
    std::string file_path = R"(C:\User\win_user_1\)" + file_name;

    trie.insert_file_path(file_name, file_path);

    auto r = trie.search("my_file_1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    r = trie.search("m");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    r = trie.search("my");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    r = trie.search("file");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    r = trie.search("_");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    r = trie.search("1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path);

    trie.erase_file_path(file_name, file_path);

    r = trie.search("");
    ASSERT_TRUE(r.size() == 0);

    r = trie.search("my_file_1");
    ASSERT_TRUE(r.size() == 0);
}

TEST(fs_trie_tests, sanity_test_2)
{
    FS_trie trie;

    std::string file_name = "attach.cpp";

    std::string file_path_1 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\linux_and_mac\)" +
        file_name;

    std::string file_path_2 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\windows\)" +
        file_name;

    trie.insert_file_path(file_name, file_path_1);
    trie.insert_file_path(file_name, file_path_2);

    auto r = trie.search("attach.cpp");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    r = trie.search("attach");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    r = trie.search("cpp");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    r = trie.search(".");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    trie.erase_file_path(file_name, file_path_1);

    r = trie.search("attach.cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path_2);

    r = trie.search("cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(*r[0] == file_path_2);
}

TEST(fs_trie_tests, file_system_paths)
{
    FS_trie trie;

    std::ifstream in_file_stream{std::string(PROJECT_ROOT) +
                                 "/test/input_files/windows_paths_vscode.txt"};
    ASSERT_TRUE(in_file_stream.is_open());

    sz cpp_files_count = 0;

    for (std::string file_path; std::getline(in_file_stream, file_path);) {
        auto path = std::filesystem::path(file_path);

        auto file_name = path.filename().string();
        if (file_name.find("cpp") != std::string::npos)
            ++cpp_files_count;

        trie.insert_file_path(path);
    }

    ASSERT_TRUE(trie.search("cpp").size() == cpp_files_count);
}

void test_fs_search(const std::string& file_name)
{
    FS_trie trie;

    std::ifstream in_file_stream{std::string(PROJECT_ROOT) + "/test/input_files/" + file_name};
    ASSERT_TRUE(in_file_stream.is_open());

    std::vector<std::string> paths;

    {
        Stopwatch<true, milliseconds> s{"Scanning"};
        for (std::string file_path; std::getline(in_file_stream, file_path);) {
            paths.emplace_back(std::move(file_path));
            trie.insert_file_path(paths.back());
        }
    }

    std::cout << "Paths count: " << paths.size() << "\n";

    constexpr sz MB = 1024 * 1024;

    std::cout << std::format("Files size:    {}MB\n", trie.size() / MB);
    // std::cout << std::format("File finder size:    {}MB\n", files.file_finder_size() / MB);
    // std::cout << std::format("Refs size:    {}MB\n", files.files_refs_size() / MB);

    std::string str_for_match;
    while (true) {
        std::cout << ": ";
        std::cin >> str_for_match;

        Stopwatch<true, microseconds> s{"Search + print"};
        auto r = trie.search(str_for_match);
        auto elapsed = s.elapsed();

        for (auto* path : r)
            std::cout << *path << "\n";

        std::cout << "Search time: " << duration_cast<microseconds>(elapsed) << "\n";
    }
}

TEST(fs_trie_tests, fs_search)
{
    GTEST_SKIP() << "Skipping filesystem path search test, because it is for manual testing only.";

    std::vector<std::string> file_names{
        "linux_paths.txt",
        "windows_paths.txt",
        "windows_paths_vscode.txt",
    };

    for (const auto& file_name : file_names)
        test_fs_search(file_name);
}

// NOLINTEND
