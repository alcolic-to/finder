#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "suffix_trie.h"
#include "test_util.h"

// NOLINTBEGIN

using namespace suffix_trie;

TEST(suffix_trie_tests, sanity_test_1)
{
    Suffix_trie s;

    s.insert_suffix("banana");

    ASSERT_TRUE(s.search_suffix("banana")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("anana")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("nana")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("ana")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("na")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("a")[0]->first == "banana");
    ASSERT_TRUE(s.search_suffix("")[0]->first == "banana");

    ASSERT_TRUE(s.search_prefix("ba").size() == 1);
    ASSERT_TRUE(s.search_prefix("a").size() == 1);
    ASSERT_TRUE(s.search_prefix("an").size() == 1);
    ASSERT_TRUE(s.search_prefix("ana").size() == 1);
    ASSERT_TRUE(s.search_prefix("n").size() == 1);
}

TEST(suffix_trie_tests, sanity_test_2)
{
    Suffix_trie s;

    s.insert_suffix("banana");
    s.insert_suffix("ana");

    auto r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "banana");

    r = s.search_suffix("anana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "banana");

    r = s.search_suffix("nana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana");
    ASSERT_TRUE(r[1]->first == "ana");

    r = s.search_suffix("na");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana");
    ASSERT_TRUE(r[1]->first == "ana");

    r = s.search_suffix("a");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana");
    ASSERT_TRUE(r[1]->first == "ana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana");
    ASSERT_TRUE(r[1]->first == "ana");
}

TEST(suffix_trie_tests, sanity_test_3)
{
    Suffix_trie s;

    s.insert_suffix("banana");
    s.insert_suffix("ana");
    s.insert_suffix("not_banana");

    auto r = s.search_suffix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "not_banana");

    r = s.search_suffix("ot_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "not_banana");

    r = s.search_suffix("t_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "not_banana");

    r = s.search_suffix("_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "not_banana");

    r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "not_banana");

    r = s.search_suffix("anana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "not_banana");

    r = s.search_suffix("nana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "not_banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "ana" && r[2]->first == "not_banana");

    r = s.search_suffix("na");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "ana" && r[2]->first == "not_banana");

    r = s.search_suffix("a");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "ana" && r[2]->first == "not_banana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->first == "banana" && r[1]->first == "ana" && r[2]->first == "not_banana");

    r = s.search_prefix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "not_banana");

    r = s.search_prefix("banana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana" || r[1]->first == "banana");
    ASSERT_TRUE(r[0]->first == "not_banana" || r[1]->first == "not_banana");

    r = s.search_prefix("b");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->first == "banana" || r[1]->first == "banana");
    ASSERT_TRUE(r[0]->first == "not_banana" || r[1]->first == "not_banana");

    r = s.search_prefix("");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->first == "banana" || r[1]->first == "banana" || r[2]->first == "banana");
    ASSERT_TRUE(r[0]->first == "not_banana" || r[1]->first == "not_banana" ||
                r[2]->first == "not_banana");
    ASSERT_TRUE(r[0]->first == "ana" || r[1]->first == "ana" || r[2]->first == "ana");
}

TEST(suffix_trie_tests, sanity_test_4)
{
    Suffix_trie<std::string> s;

    std::string file_path =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printing)";

    s.insert_suffix("printing", file_path);

    auto r = s.search_suffix("printing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    r = s.search_suffix("in");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("in");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    r = s.search_suffix("ing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    r = s.search_prefix("ing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    r = s.search_suffix("p");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("p");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    s.insert_suffix("printige", file_path);

    r = s.search_suffix("printing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->first == "printing");
    ASSERT_TRUE(r[0]->second == file_path);

    r = s.search_suffix("in");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("in");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->second == file_path);
    ASSERT_TRUE(r[1]->second == file_path);
    ASSERT_TRUE(r[0]->first == "printing" || r[1]->first == "printing");
    ASSERT_TRUE(r[0]->first == "printige" || r[1]->first == "printige");

    r = s.search_prefix("print");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->second == file_path);
    ASSERT_TRUE(r[1]->second == file_path);
    ASSERT_TRUE(r[0]->first == "printing" || r[1]->first == "printing");
    ASSERT_TRUE(r[0]->first == "printige" || r[1]->first == "printige");

    r = s.search_prefix("printi");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->second == file_path);
    ASSERT_TRUE(r[1]->second == file_path);
    ASSERT_TRUE(r[0]->first == "printing" || r[1]->first == "printing");
    ASSERT_TRUE(r[0]->first == "printige" || r[1]->first == "printige");

    r = s.search_prefix("p");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->second == file_path);
    ASSERT_TRUE(r[1]->second == file_path);
    ASSERT_TRUE(r[0]->first == "printing" || r[1]->first == "printing");
    ASSERT_TRUE(r[0]->first == "printige" || r[1]->first == "printige");
}

// Reads all filesystem paths from provided input file, inserts them into Suffix trie and search for
// them 1 by 1 while verifying searches.
//
void test_filesystem_paths(const std::string& file_name)
{
    Suffix_trie trie;

    std::ifstream in_file_stream{"test/input_files/" + file_name};
    std::vector<std::string> paths;

    for (std::string file_path; std::getline(in_file_stream, file_path);) {
        paths.push_back(file_path);
        // trie.insert_path(paths.back());
    }

    // TODO: Do test_crud here.
    // It seems that there are duplicates in files.
    // for (auto& it : paths) {

    std::string str_for_match;
    while (true) {
        std::cout << "String for match: ";
        std::cin >> str_for_match;

        // auto r = trie.find_prefix(str_for_match);
        // for (auto& leaf : r)
        //     for (auto& suffix : leaf->value())
        //         std::cout << *suffix << "\n";
    }

    // ASSERT_TRUE(r->value().size() == 10000);
    // }

    // constexpr size_t MB = 1024 * 1024;

    // std::cout << std::format("\nEntries count:           {}K\n", paths.size() / 1000);
    // std::cout << std::format("ART size with leaves:    {}MB\n", art.size_in_bytes(true) /
    // MB); std::cout << std::format("ART size without leaves: {}MB\n\n",
    // art.size_in_bytes(false) / MB);
}

TEST(suffix_trie_tests, file_system_paths)
{
    std::vector<std::string> file_names{
        // "windows_paths.txt",
        // "linux_paths.txt",
        "windows_paths_vscode.txt",
    };

    for (const auto& file_name : file_names)
        test_filesystem_paths(file_name);
}

// NOLINTEND
