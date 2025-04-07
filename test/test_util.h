
#include <gtest/gtest.h>
#include <iterator>
#include <string>

#include "art.h"

// NOLINTBEGIN

using Keys = const std::vector<std::string>&;

template<class T>
void assert_search(const art::ART<T>& art, const std::string& key,
                   typename art::ART<T>::const_reference value = T{})
{
    art::Leaf<T>* leaf = art.search(key);
    ASSERT_TRUE(leaf != nullptr && key == leaf->key_to_string() && leaf->value() == value);
}

template<class T>
void assert_failed_search(art::ART<T>& art, const std::string& key)
{
    ASSERT_TRUE(art.search(key) == nullptr);
}

template<class T>
void test_insert(art::ART<T>& art, Keys insert_keys, Keys valid_keys, Keys invalid_keys)
{
    for (auto it = insert_keys.begin(); it != insert_keys.end(); ++it) {
        art.insert(*it);
        assert_search(art, *it);

        for (auto it_s = insert_keys.begin(); it_s != it; ++it_s)
            assert_search(art, *it_s);

        for (auto it_s = std::next(it); it_s != insert_keys.end(); ++it_s)
            assert_failed_search(art, *it_s);

        for (auto& it_val : valid_keys)
            assert_search(art, it_val);

        for (auto& it_inv : invalid_keys)
            assert_failed_search(art, it_inv);
    }
}

template<class T>
void test_erase(art::ART<T>& art, Keys erase_keys, Keys valid_keys, Keys invalid_keys)
{
    for (auto it = erase_keys.begin(); it != erase_keys.end(); ++it) {
        art.erase(*it);
        assert_failed_search(art, *it);

        for (auto it_s = erase_keys.begin(); it_s != it; ++it_s)
            assert_failed_search(art, *it_s);

        for (auto it_s = std::next(it); it_s != erase_keys.end(); ++it_s)
            assert_search(art, *it_s);

        for (auto& it_val : valid_keys)
            assert_search(art, it_val);

        for (auto& it_inv : invalid_keys)
            assert_failed_search(art, it_inv);
    }
}

template<class T>
void test_crud(art::ART<T>& art, Keys keys, Keys valid_keys, Keys invalid_keys)
{
    test_insert(art, keys, valid_keys, invalid_keys);
    test_erase(art, keys, valid_keys, invalid_keys);
}

// NOLINTEND
