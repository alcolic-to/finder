#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "suffix_trie.h"
#include "test_util.h"

// NOLINTBEGIN

TEST(suffix_trie_tests, sanity_test_1)
{
    Suffix_trie s;

    s.insert_suffix("banana");
    ASSERT_TRUE(**s.find_suffix("banana")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("anana")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("nana")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("ana")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("na")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("a")->value().begin() == "banana");
    ASSERT_TRUE(**s.find_suffix("")->value().begin() == "banana");

    ASSERT_TRUE(s.find_prefix("ba").size() == 1);
    ASSERT_TRUE(s.find_prefix("a").size() == 3);
    ASSERT_TRUE(s.find_prefix("an").size() == 2);
    ASSERT_TRUE(s.find_prefix("ana").size() == 2);
}

TEST(suffix_trie_tests, sanity_test_2)
{
    Suffix_trie s;

    s.insert_suffix("banana");
    s.insert_suffix("ana");

    auto r = s.find_suffix("banana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "banana");

    r = s.find_suffix("anana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "banana");

    r = s.find_suffix("nana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "banana");

    r = s.find_suffix("ana")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana");
    ASSERT_TRUE(**(r.begin() + 1) == "ana");

    r = s.find_suffix("na")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana");
    ASSERT_TRUE(**(r.begin() + 1) == "ana");

    r = s.find_suffix("a")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana");
    ASSERT_TRUE(**(r.begin() + 1) == "ana");

    r = s.find_suffix("")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana");
    ASSERT_TRUE(**(r.begin() + 1) == "ana");
}

TEST(suffix_trie_tests, sanity_test_3)
{
    Suffix_trie s;

    s.insert_suffix("banana");
    s.insert_suffix("ana");
    s.insert_suffix("not_banana");

    auto r = s.find_suffix("not_banana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "not_banana");

    r = s.find_suffix("ot_banana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "not_banana");

    r = s.find_suffix("t_banana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "not_banana");

    r = s.find_suffix("_banana")->value();
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(**r.begin() == "not_banana");

    r = s.find_suffix("banana")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "not_banana");

    r = s.find_suffix("anana")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "not_banana");

    r = s.find_suffix("nana")->value();
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "not_banana");

    r = s.find_suffix("ana")->value();
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "ana" &&
                **(r.begin() + 2) == "not_banana");

    r = s.find_suffix("na")->value();
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "ana" &&
                **(r.begin() + 2) == "not_banana");

    r = s.find_suffix("a")->value();
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "ana" &&
                **(r.begin() + 2) == "not_banana");

    r = s.find_suffix("")->value();
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(**r.begin() == "banana" && **(r.begin() + 1) == "ana" &&
                **(r.begin() + 2) == "not_banana");
}

TEST(suffix_trie_tests, sanity_test_4)
{
    Suffix_trie s;

    s.insert_suffix(
        R"(C:\Users\topac\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printing)");

    auto r = s.find_prefix("in");

    for (auto& leaf : r)
        for (const auto& suffix : leaf->value())
            std::cout << *suffix << "\n";
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
        trie.insert_path(paths.back());
    }

    // TODO: Do test_crud here.
    // It seems that there are duplicates in files.
    // for (auto& it : paths) {

    std::string str_for_match;
    while (true) {
        std::cout << "String for match: ";
        std::cin >> str_for_match;

        auto r = trie.find_prefix(str_for_match);
        for (auto& leaf : r)
            for (auto& suffix : leaf->value())
                std::cout << *suffix << "\n";
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
