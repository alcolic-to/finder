#include <gtest/gtest.h>

#include "art.h"

TEST(art_tests, sanity_test)
{
    ART art;

    std::string s{"a"};
    art.insert(s);

    Leaf* leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());

    s = "";
    leaf = art.search(s);
    ASSERT_TRUE(leaf == nullptr);

    s = "aa";
    leaf = art.search(s);
    ASSERT_TRUE(leaf == nullptr);

    s = "b";
    leaf = art.search(s);
    ASSERT_TRUE(leaf == nullptr);
}

TEST(art_tests, multiple_insertions)
{
    ART art;

    std::string s{"aaaaaa"};
    art.insert(s);

    s = "aaaaa";
    art.insert(s);

    s = "a";
    art.insert(s);

    s = "aaaaaaaa";
    art.insert(s);

    Leaf* leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());

    s = "a", leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());

    s = "aaaaa", leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());

    s = "aaaaaa", leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());
}

TEST(art_tests, long_keys_insertion)
{
    ART art;

    // clang-format off
    std::string full     {"abcdefghijklmnopqrstuvwxyz"};
    std::string partial_1{"abcdefghijklmnopqrstuvwxy"};
    std::string partial_2{"abcdefghijklmnopqrstuvwxyza"};
    // clang-format on

    // art.insert(full);

    // s = "aaaaa";
    // art.insert(s);

    // s = "a";
    // art.insert(s);

    // s = "aaaaaaaa";
    // art.insert(s);

    // Leaf* leaf = art.search(s);
    // ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string());
}
