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
