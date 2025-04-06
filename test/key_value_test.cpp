#include <cstddef>
#include <format>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "art.h"

// NOLINTBEGIN

using namespace art;

template<class T>
void assert_failed_search(ART<T>& art, const std::string& s)
{
    ASSERT_TRUE(art.search(s) == nullptr);
}

template<class T>
void assert_search(const ART<T>& art, const std::string& s, ART<T>::const_reference value)
{
    Leaf<T>* leaf = art.search(s);
    ASSERT_TRUE(leaf != nullptr && s == leaf->key_to_string() && leaf->value() == value);
}

using Keys = const std::vector<std::string>&;

TEST(art_key_value_tests, sanity_test_1)
{
    art::ART<std::string> art_str;

    std::string s{"string_1"};

    art_str.insert("key_1", s);
    assert_search(art_str, "key_1", s);
}

// NOLINTEND
