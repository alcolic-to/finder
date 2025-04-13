#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "fs_trie.h"
#include "test_util.h"

// NOLINTBEGIN

TEST(fs_trie_tests, sanity_test_1)
{
    FS_trie trie;

    const std::string filename = "my_file_1";
    std::string filepath = R"(C:\User\win_user_1\)" + filename;

    trie.insert_file_path(filename, filepath);

    auto r = trie.search("my_file_1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    r = trie.search("m");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    r = trie.search("my");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    r = trie.search("file");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    r = trie.search("_");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    r = trie.search("1");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == filename && r[0]->second[0] == filepath);

    trie.erase_file_path(filename, filepath);

    ASSERT_TRUE(trie.$().size() == 0);

    r = trie.search("my_file_1");
    ASSERT_TRUE(r.size() == 0);
}

TEST(fs_trie_tests, sanity_test_2)
{
    FS_trie trie;

    std::string filename_1 = "attach.cpp";
    std::string filepath_1 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\linux_and_mac\)" +
        filename_1;

    std::string filepath_2 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\windows\)" +
        filename_1;

    trie.insert_file_path(filename_1, filepath_1);
    trie.insert_file_path(filename_1, filepath_2);

    auto r = trie.search("attach.cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->second.size() == 2);

    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second[0] == filepath_1 &&
                r[0]->second[1] == filepath_2);

    r = trie.search("attach");
    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second[0] == filepath_1 &&
                r[0]->second[1] == filepath_2);

    r = trie.search("cpp");
    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second[0] == filepath_1 &&
                r[0]->second[1] == filepath_2);

    r = trie.search(".");
    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second[0] == filepath_1 &&
                r[0]->second[1] == filepath_2);

    trie.erase_file_path(filename_1, filepath_1);

    r = trie.search("attach.cpp");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->second.size() == 1);

    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second.size() == 1 &&
                r[0]->second[0] == filepath_2);

    r = trie.search("cpp");
    ASSERT_TRUE(r[0]->first == filename_1 && r[0]->second.size() == 1 &&
                r[0]->second[0] == filepath_2);
}

TEST(fs_trie_tests, file_system_paths)
{
    FS_trie trie;

    std::ifstream in_file_stream{std::string(PROJECT_ROOT) +
                                 "/test/input_files/windows_paths_vscode.txt"};
    ASSERT_TRUE(in_file_stream.is_open());

    for (std::string file_path; std::getline(in_file_stream, file_path);)
        trie.insert_file_path(file_path);

    ASSERT_TRUE(trie.search("cpp").size() == 26);
}

// Reads all filesystem paths from provided input file, inserts them into trie and search for
// them 1 by 1 while verifying searches.
//
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

    std::string str_for_match;
    while (true) {
        std::cout << ": ";
        std::cin >> str_for_match;

        Stopwatch<true, microseconds> s{"Search + print"};
        auto r = trie.search(str_for_match);
        auto elapsed = s.elapsed();

        for (auto& entry : r) {
            // std::cout << "File name: " << entry->first << "\n";
            for (auto& path : entry->second)
                std::cout << path << "\n";
        }

        std::cout << "Search time: " << duration_cast<microseconds>(elapsed) << "\n";
    }

    // ASSERT_TRUE(r->value().size() == 10000);
    // }

    // constexpr size_t MB = 1024 * 1024;

    // std::cout << std::format("\nEntries count:           {}K\n", paths.size() / 1000);
    // std::cout << std::format("ART size with leaves:    {}MB\n", art.size_in_bytes(true) /
    // MB); std::cout << std::format("ART size without leaves: {}MB\n\n",
    // art.size_in_bytes(false) / MB);
}

TEST(fs_trie_tests, fs_search)
{
    GTEST_SKIP() << "Skipping filesystem path search test, because it is for testing only.";

    std::vector<std::string> file_names{
        "linux_paths.txt",
        "windows_paths.txt",
        "windows_paths_vscode.txt",
    };

    for (const auto& file_name : file_names)
        test_fs_search(file_name);
}

// NOLINTEND
