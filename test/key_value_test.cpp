#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "art.h"
#include "test_util.h"

// NOLINTBEGIN

TEST(art_key_value_tests, sanity_test_1)
{
    art::ART<std::string> art_str;

    std::string key{"key_1"};
    std::string value{"value_1"};

    art_str.insert(key, value);
    assert_search(art_str, key, value);

    art_str.insert("key_2", "value_2");
    assert_search(art_str, "key_2", "value_2");

    std::string key_3{"key_3"};
    std::string value_3{"value_3"};

    art_str.insert(std::move(key_3), std::move(value_3));
    assert_search(art_str, "key_3", "value_3");
    ASSERT_TRUE(key_3 == "key_3" && value_3.empty()); // key should not be moved.
}

TEST(art_key_value_tests, sanity_test_2)
{
    art::ART<std::vector<i32>> art_v;

    std::vector<i32> v1{1, 2, 3, 4, 5, 6};
    art_v.insert("key_1", v1);
    assert_search(art_v, "key_1", v1);

    auto leaf = art_v.search("key_1");
    leaf->value().pop_back();

    auto leaf_1 = art_v.search("key_1");
    ASSERT_TRUE(leaf_1->value() != v1);

    v1.pop_back();
    assert_search(art_v, "key_1", v1);

    leaf->value() = {1, 2, 3};
    assert_search(art_v, "key_1", {1, 2, 3});
}

TEST(art_key_value_tests, sanity_test_3)
{
    art::ART<std::string> art_s;

    ASSERT_TRUE(art_s.insert("key_1", "value_1"));
    auto res = art_s.insert("key_1", "value_2");

    ASSERT_TRUE(res == false && res->value() == "value_1");
    res->value() = "value_2";

    ASSERT_TRUE(art_s.search("key_1")->value() == "value_2");
}

// NOLINTEND
