#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>
#include <limits>

#include "ast.h"
#include "test_util.h"

// NOLINTBEGIN

using namespace ast;

TEST(suffix_trie_tests, sanity_test_1)
{
    AST<std::string> ast;

    ast.insert("banana", "banana");

    ASSERT_TRUE(ast.search("banana")->str() == "banana");
    ASSERT_TRUE(ast.search("banana")->value() == "banana");
    ASSERT_TRUE(ast.search("anana") == nullptr);

    ASSERT_TRUE(ast.search_suffix("banana")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("anana")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("nana")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("ana")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("na")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("a")[0]->value() == "banana");
    ASSERT_TRUE(ast.search_suffix("")[0]->value() == "banana");

    ASSERT_TRUE(ast.search_prefix("ba").size() == 1);
    ASSERT_TRUE(ast.search_prefix("a").size() == 1);
    ASSERT_TRUE(ast.search_prefix("an").size() == 1);
    ASSERT_TRUE(ast.search_prefix("ana").size() == 1);
    ASSERT_TRUE(ast.search_prefix("n").size() == 1);

    ast.erase("banana");

    ASSERT_TRUE(ast.search_suffix("banana").empty());
    ASSERT_TRUE(ast.search_prefix("banana").empty());
}

TEST(suffix_trie_tests, sanity_test_2)
{
    AST<std::string> ast;

    ast.insert("banana", "banana");
    ast.insert("ana", "ana");

    auto r = ast.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "banana");

    r = ast.search_suffix("anana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "banana");

    r = ast.search_suffix("nana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "banana");

    r = ast.search_suffix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_suffix("na");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_suffix("a");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_suffix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    ast.erase("ana");

    r = ast.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("anana");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("nana");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("ana");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("na");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("a");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    r = ast.search_suffix("");
    ASSERT_TRUE(r.size() == 1 && r[0]->value() == "banana");

    ASSERT_TRUE(ast.search_prefix("ba").size() == 1);
    ASSERT_TRUE(ast.search_prefix("a").size() == 1);
    ASSERT_TRUE(ast.search_prefix("an").size() == 1);
    ASSERT_TRUE(ast.search_prefix("ana").size() == 1);
    ASSERT_TRUE(ast.search_prefix("n").size() == 1);

    ast.erase("banana");

    ASSERT_TRUE(ast.search_suffix("banana").empty());
    ASSERT_TRUE(ast.search_prefix("banana").empty());

    ASSERT_TRUE(ast.search_suffix("ana").empty());
    ASSERT_TRUE(ast.search_prefix("ana").empty());
}

TEST(suffix_trie_tests, sanity_test_3)
{
    AST<std::string> ast;

    ast.insert("banana", "banana");
    ast.insert("ana", "ana");
    ast.insert("not_banana", "not_banana");

    auto r = ast.search_suffix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("ot_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("t_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("banana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");

    r = ast.search_suffix("anana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");

    r = ast.search_suffix("nana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");

    r = ast.search_suffix("ana");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana" ||
                r[2]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana" ||
                r[2]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana" || r[2]->value() == "ana");

    r = ast.search_suffix("na");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana" ||
                r[2]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana" ||
                r[2]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana" || r[2]->value() == "ana");

    r = ast.search_suffix("a");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana" ||
                r[2]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana" ||
                r[2]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana" || r[2]->value() == "ana");

    r = ast.search_suffix("");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->value() == "banana" || r[1]->value() == "banana" ||
                r[2]->value() == "banana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana" ||
                r[2]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana" || r[2]->value() == "ana");

    auto r2 = ast.search_prefix("not_banana");
    ASSERT_TRUE(r2.size() == 1);
    ASSERT_TRUE(r2[0]->value() == "not_banana");

    r2 = ast.search_prefix("banana");
    ASSERT_TRUE(r2.size() == 2);
    ASSERT_TRUE(r2[0]->value() == "banana" || r2[1]->value() == "banana");
    ASSERT_TRUE(r2[0]->value() == "not_banana" || r2[1]->value() == "not_banana");

    r2 = ast.search_prefix("b");
    ASSERT_TRUE(r2.size() == 2);
    ASSERT_TRUE(r2[0]->value() == "banana" || r2[1]->value() == "banana");
    ASSERT_TRUE(r2[0]->value() == "not_banana" || r2[1]->value() == "not_banana");

    r2 = ast.search_prefix("");
    ASSERT_TRUE(r2.size() == 3);
    ASSERT_TRUE(r2[0]->value() == "banana" || r2[1]->value() == "banana" ||
                r2[2]->value() == "banana");
    ASSERT_TRUE(r2[0]->value() == "not_banana" || r2[1]->value() == "not_banana" ||
                r2[2]->value() == "not_banana");
    ASSERT_TRUE(r2[0]->value() == "ana" || r2[1]->value() == "ana" || r2[2]->value() == "ana");

    ast.erase("banana");

    r = ast.search_suffix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_suffix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");

    r = ast.search_suffix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");

    r = ast.search_prefix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_prefix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_prefix("an");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_prefix("n");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    r = ast.search_prefix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == "not_banana" || r[1]->value() == "not_banana");
    ASSERT_TRUE(r[0]->value() == "ana" || r[1]->value() == "ana");

    ast.erase("ana");

    r = ast.search_prefix("ana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    r = ast.search_prefix("");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == "not_banana");

    ast.erase("not_banana");

    r = ast.search_prefix("");
    ASSERT_TRUE(r.size() == 0);
}

TEST(suffix_trie_tests, sanity_test_4)
{
    AST<std::string> ast;

    std::string long_str = "aaaaaaaaaaaaaaaaaaaa";

    ast.insert(long_str, long_str);

    auto r = ast.search("aaaaaaaaaaaaaaaaaaaa");
    ASSERT_TRUE(r != nullptr && r->value() == "aaaaaaaaaaaaaaaaaaaa" &&
                r->value() == "aaaaaaaaaaaaaaaaaaaa");

    auto r1 = ast.search_suffix("aaaaaaaaaaaaaaaaaaaa");
    ASSERT_TRUE(r1.size() == 1);
    ASSERT_TRUE(r1[0]->value() == "aaaaaaaaaaaaaaaaaaaa");

    auto r2 = ast.search_prefix("aaaaaaaaaaaaaaaaaaaa");
    ASSERT_TRUE(r2.size() == 1);
    ASSERT_TRUE(r2[0]->value() == "aaaaaaaaaaaaaaaaaaaa");

    // for (u32 i = 0; i < std::numeric_limits<u32>::max(); ++i) {
    //     ast.insert("file_name_file_name_file_name_file_name_file_name_" + std::to_string(i));
    //     if (i % (1024 * 128) == 0) {
    //         // std::cout << "Entries: " << i << "\n";
    //         std::cout << "Entries: " << i << ", size: " << ast.size_in_bytes() / 1024 / 1024
    //                   << "MB\n";
    //     }
    // }
}

TEST(suffix_trie_tests, sanity_test_5)
{
    AST<std::string> s;

    std::string file_path_1 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printing)";

    std::string file_path_2 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printige)";

    s.insert("printing", file_path_1);

    auto r = s.search_suffix("printing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    r = s.search_suffix("in");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("in");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    r = s.search_suffix("ing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    r = s.search_prefix("ing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    r = s.search_suffix("p");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("p");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    s.insert("printige", file_path_2);

    r = s.search_suffix("printing");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_1);

    r = s.search_suffix("in");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("in");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == file_path_1 || r[1]->value() == file_path_1);
    ASSERT_TRUE(r[0]->value() == file_path_2 || r[1]->value() == file_path_2);

    r = s.search_prefix("print");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == file_path_1 || r[1]->value() == file_path_1);
    ASSERT_TRUE(r[0]->value() == file_path_2 || r[1]->value() == file_path_2);

    r = s.search_prefix("printi");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == file_path_1 || r[1]->value() == file_path_1);
    ASSERT_TRUE(r[0]->value() == file_path_2 || r[1]->value() == file_path_2);

    r = s.search_prefix("p");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->value() == file_path_1 || r[1]->value() == file_path_1);
    ASSERT_TRUE(r[0]->value() == file_path_2 || r[1]->value() == file_path_2);

    s.erase("printing");

    r = s.search_suffix("printing");
    ASSERT_TRUE(r.empty());

    r = s.search_suffix("in");
    ASSERT_TRUE(r.size() == 0);

    r = s.search_prefix("in");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_2);

    r = s.search_prefix("print");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_2);

    r = s.search_prefix("printi");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_2);

    r = s.search_prefix("p");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_2);

    r = s.search_prefix("");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->value() == file_path_2);
}

// NOLINTEND
