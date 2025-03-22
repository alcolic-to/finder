#include <gtest/gtest.h>

#include "art.h"

void assert_failed_search(ART& art, const std::string& s)
{
    Leaf* leaf = art.search(s);
    ASSERT_TRUE(leaf == nullptr);
}

void assert_search(ART& art, const std::string& s)
{
    Leaf* leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());
}

TEST(art_tests, sanity_test)
{
    ART art;

    art.insert("a");

    assert_search(art, "a");

    assert_failed_search(art, "");
    assert_failed_search(art, "aa");
    assert_failed_search(art, "b");
}

TEST(art_tests, multiple_insertions)
{
    ART art;

    art.insert("aaaaaa");
    art.insert("aaaaa");
    art.insert("a");
    art.insert("aaaaaaaa");

    assert_search(art, "aaaaaaaa");
    assert_search(art, "a");
    assert_search(art, "aaaaa");
    assert_search(art, "aaaaaaaa");

    assert_failed_search(art, "aaaaaaaaa");
}

TEST(art_tests, similar_keys_insertion)
{
    ART art;

    art.insert("aaaa");
    art.insert("aaaaa");
    art.insert("a");
    art.insert("aaaaaaaaaa");
    art.insert("aaba");
    art.insert("aa");
    art.insert("a");

    assert_search(art, "aaaa");
    assert_search(art, "aaaaa");
    assert_search(art, "a");
    assert_search(art, "aaaaaaaaaa");
    assert_search(art, "aaba");
    assert_search(art, "aa");

    assert_failed_search(art, "aaa");
}

TEST(art_tests, long_keys_insertion)
{
    ART art;

    // art.insert("a");
    // art.insert("aa");
    // art.insert("aaa");
    // art.insert("aaaa");

    art.insert("aaaa");
    art.insert("aaaaa");
    art.insert("a");
    art.insert("aaaaaaaaaa");
    art.insert("aaba");
    art.insert("aa");
    // art.insert("a");

    // clang-format off
    // std::string full     {"abcdefghijklmnopqrstuvwxyz"};
    // std::string partial_1{"abcdefghijklmnopqrstuvwxy"};
    // std::string partial_2{"abcdefghijklmnopqrstuvwxyza"};
    // std::string partial_3{"abcdefghiaaaaaapqrstuvwxyza"};
    // std::string partial_4{"abcdefghijaaaaapqrstuvwxyza"};

    // clang-format on

    // art.insert(full);
    // art.insert(partial_1);
    // art.insert(partial_3);

    // Leaf* leaf = art.search(full);
    // ASSERT_TRUE(leaf != nullptr && full == leaf->key_to_string());

    // leaf = art.search(partial_1);
    // ASSERT_TRUE(leaf != nullptr && partial_1 == leaf->key_to_string());

    // leaf = art.search(partial_3);
    // ASSERT_TRUE(leaf != nullptr && partial_3 == leaf->key_to_string());

    // art.insert("aaaaaaaaa");
    // art.insert("aaaaaaaaaa");
    // art.insert("aaaaaaaab");
    // art.insert("aaaaaaaaab");

    // s = "aaaaa";
    // art.insert(s);

    // s = "a";
    // art.insert(s);

    // s = "aaaaaaaa";
    // art.insert(s);

    // Leaf* leaf = art.search(s);
    // ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());
}
