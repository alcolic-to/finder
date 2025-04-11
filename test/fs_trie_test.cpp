#include <cstddef>
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
    std::string filepath = R"(C:\User\aca\)" + filename;

    trie.insert_file_path(filename, filepath);
    trie.erase_file_path(filename, filepath);
}

TEST(fs_trie_tests, sanity_test_2)
{
    FS_trie trie;

    std::string filename_1 = "attach.cpp";
    std::string filepath_1 =
        R"(C:\Users\topac\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\linux_and_mac\)" +
        filename_1;

    std::string filename_2 = "attach.cpp";
    std::string filepath_2 =
        R"(C:\Users\topac\.vscode\extensions\ms-python.debugpy-2025.6.0-win32-x64\bundled\libs\debugpy\_vendored\pydevd\pydevd_attach_to_process\windows\)" +
        filename_2;

    trie.insert_file_path(filename_1, filepath_1);
    trie.insert_file_path(filename_2, filepath_2);

    std::pair<std::string, std::string> s;

    // trie.erase_file_path(filename, filepath);
}

// NOLINTEND
