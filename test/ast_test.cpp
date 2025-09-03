#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "ast.h"
#include "test_util.h"

// NOLINTBEGIN

using namespace ast;

TEST(suffix_trie_tests, sanity_test_1)
{
    AST<std::string> s;

    s.insert("banana");

    ASSERT_TRUE(s.search("banana")->str() == "banana");
    ASSERT_TRUE(s.search("anana") == nullptr);

    ASSERT_TRUE(s.search_suffix("banana")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("anana")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("nana")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("ana")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("na")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("a")[0]->str() == "banana");
    ASSERT_TRUE(s.search_suffix("")[0]->str() == "banana");

    ASSERT_TRUE(s.search_prefix("ba").size() == 1);
    ASSERT_TRUE(s.search_prefix("a").size() == 1);
    ASSERT_TRUE(s.search_prefix("an").size() == 1);
    ASSERT_TRUE(s.search_prefix("ana").size() == 1);
    ASSERT_TRUE(s.search_prefix("n").size() == 1);

    s.erase("banana");

    ASSERT_TRUE(s.search_suffix("banana").empty());
    ASSERT_TRUE(s.search_prefix("banana").empty());
}

TEST(suffix_trie_tests, sanity_test_2)
{
    AST<std::string> s;

    s.insert("banana");
    s.insert("ana");

    auto r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "banana");

    r = s.search_suffix("anana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "banana");

    r = s.search_suffix("nana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_suffix("na");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_suffix("a");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    s.erase("ana");

    r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("anana");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("nana");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("na");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("a");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 1 && r[0]->str() == "banana");

    ASSERT_TRUE(s.search_prefix("ba").size() == 1);
    ASSERT_TRUE(s.search_prefix("a").size() == 1);
    ASSERT_TRUE(s.search_prefix("an").size() == 1);
    ASSERT_TRUE(s.search_prefix("ana").size() == 1);
    ASSERT_TRUE(s.search_prefix("n").size() == 1);

    s.erase("banana");

    ASSERT_TRUE(s.search_suffix("banana").empty());
    ASSERT_TRUE(s.search_prefix("banana").empty());

    ASSERT_TRUE(s.search_suffix("ana").empty());
    ASSERT_TRUE(s.search_prefix("ana").empty());
}

TEST(suffix_trie_tests, sanity_test_3)
{
    AST<std::string> s;

    s.insert("banana");
    s.insert("ana");
    s.insert("not_banana");

    auto r = s.search_suffix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("ot_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("t_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");

    r = s.search_suffix("anana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");

    r = s.search_suffix("nana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana" || r[2]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana" ||
                r[2]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana" || r[2]->str() == "ana");

    r = s.search_suffix("na");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana" || r[2]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana" ||
                r[2]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana" || r[2]->str() == "ana");

    r = s.search_suffix("a");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana" || r[2]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana" ||
                r[2]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana" || r[2]->str() == "ana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 3);
    ASSERT_TRUE(r[0]->str() == "banana" || r[1]->str() == "banana" || r[2]->str() == "banana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana" ||
                r[2]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana" || r[2]->str() == "ana");

    auto r2 = s.search_prefix("not_banana");
    ASSERT_TRUE(r2.size() == 1);
    ASSERT_TRUE(r2[0]->str() == "not_banana");

    r2 = s.search_prefix("banana");
    ASSERT_TRUE(r2.size() == 2);
    ASSERT_TRUE(r2[0]->str() == "banana" || r2[1]->str() == "banana");
    ASSERT_TRUE(r2[0]->str() == "not_banana" || r2[1]->str() == "not_banana");

    r2 = s.search_prefix("b");
    ASSERT_TRUE(r2.size() == 2);
    ASSERT_TRUE(r2[0]->str() == "banana" || r2[1]->str() == "banana");
    ASSERT_TRUE(r2[0]->str() == "not_banana" || r2[1]->str() == "not_banana");

    r2 = s.search_prefix("");
    ASSERT_TRUE(r2.size() == 3);
    ASSERT_TRUE(r2[0]->str() == "banana" || r2[1]->str() == "banana" || r2[2]->str() == "banana");
    ASSERT_TRUE(r2[0]->str() == "not_banana" || r2[1]->str() == "not_banana" ||
                r2[2]->str() == "not_banana");
    ASSERT_TRUE(r2[0]->str() == "ana" || r2[1]->str() == "ana" || r2[2]->str() == "ana");

    s.erase("banana");

    r = s.search_suffix("not_banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_suffix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");

    r = s.search_suffix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");

    r = s.search_prefix("banana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_prefix("ana");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_prefix("an");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_prefix("n");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    r = s.search_prefix("");
    ASSERT_TRUE(r.size() == 2);
    ASSERT_TRUE(r[0]->str() == "not_banana" || r[1]->str() == "not_banana");
    ASSERT_TRUE(r[0]->str() == "ana" || r[1]->str() == "ana");

    s.erase("ana");

    r = s.search_prefix("ana");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    r = s.search_prefix("");
    ASSERT_TRUE(r.size() == 1);
    ASSERT_TRUE(r[0]->str() == "not_banana");

    s.erase("not_banana");

    r = s.search_prefix("");
    ASSERT_TRUE(r.size() == 0);
}

TEST(suffix_trie_tests, sanity_test_4)
{
    AST<std::string> s;

    std::string file_path_1 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printing)";

    std::string file_path_2 =
        R"(C:\Users\win_user_1\.vscode\extensions\ms-python.vscode-pylance-2025.4.1\dist\bundled\stubs\sympy-stubs\printige)";

    // s.insert("printing", file_path_1);

    // auto r = s.search_suffix("printing");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // r = s.search_suffix("in");
    // ASSERT_TRUE(r.size() == 0);

    // r = s.search_prefix("in");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // r = s.search_suffix("ing");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // r = s.search_prefix("ing");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // r = s.search_suffix("p");
    // ASSERT_TRUE(r.size() == 0);

    // r = s.search_prefix("p");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // s.insert_suffix("printige", file_path_2);

    // r = s.search_suffix("printing");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_1);

    // r = s.search_suffix("in");
    // ASSERT_TRUE(r.size() == 0);

    // r = s.search_prefix("in");
    // ASSERT_TRUE(r.size() == 2);
    // ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    // ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    // r = s.search_prefix("print");
    // ASSERT_TRUE(r.size() == 2);
    // ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    // ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    // r = s.search_prefix("printi");
    // ASSERT_TRUE(r.size() == 2);
    // ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    // ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    // r = s.search_prefix("p");
    // ASSERT_TRUE(r.size() == 2);
    // ASSERT_TRUE(*r[0] == file_path_1 || *r[1] == file_path_1);
    // ASSERT_TRUE(*r[0] == file_path_2 || *r[1] == file_path_2);

    // s.erase_suffix("printing");

    // r = s.search_suffix("printing");
    // ASSERT_TRUE(r.empty());

    // r = s.search_suffix("in");
    // ASSERT_TRUE(r.size() == 0);

    // r = s.search_prefix("in");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_2);

    // r = s.search_prefix("print");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_2);

    // r = s.search_prefix("printi");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_2);

    // r = s.search_prefix("p");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_2);

    // r = s.search_prefix("");
    // ASSERT_TRUE(r.size() == 1);
    // ASSERT_TRUE(*r[0] == file_path_2);
}

// NOLINTEND