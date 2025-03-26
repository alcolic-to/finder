#include <algorithm>
#include <array>
#include <cstddef>
#include <fstream>
#include <gtest/gtest.h>
#include <iterator>

#include "art.h"

// NOLINTBEGIN

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

void assert_search(ART& art, const uint8_t* data, size_t size)
{
    Leaf* leaf = art.search(data, size);
    ASSERT_TRUE(leaf != nullptr && leaf->match(Key{data, size}));
}

using Keys = const std::vector<std::string>&;

void test_insert(ART& art, Keys insert_keys, Keys valid_keys, Keys invalid_keys)
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

void test_erase(ART& art, Keys erase_keys, Keys valid_keys, Keys invalid_keys)
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

void test_crud(Keys keys, Keys valid_keys, Keys invalid_keys)
{
    ART art;

    test_insert(art, keys, valid_keys, invalid_keys);
    test_erase(art, keys, valid_keys, invalid_keys);
}

TEST(art_tests, sanity_test)
{
    ART art;

    test_insert(art, {"a"}, {}, {"", "aa", "b"});
    test_insert(art, {""}, {"a"}, {"aa", "b"});
    test_erase(art, {"a"}, {""}, {"a", "aa", "b"});
    test_erase(art, {""}, {}, {"", "a", "aa", "b"});
}

TEST(art_tests, multiple_items)
{
    test_crud({"abcdef", "abcde", "a", "abcdefgh"}, {},
              {"", "ab", "acdef", "abcdefg", "abcdefghy"});
}

TEST(art_tests, similar_keys_insertion)
{
    test_crud({"aaaa", "aaaaa", "a", "aaaaaaaaaa", "aaba", "aa"}, {}, {"aaa"});
}

TEST(art_tests, similar_keys_insertion_2)
{
    test_crud({"a", "aa", "aaa", "aaaa", "aaaaa", "aaaaaa", "aaaaaaa"}, {},
              {"", "aaaaaaaa", "b", "ab", "aab", "aaab", "aaaab", "aaaaab", "aaaaaab", "aaaaaaab"});
}

TEST(art_tests, similar_prefix_insertions)
{
    ART art;

    test_insert(art, {"aaaaaaaaa"}, {}, {"aaaaaaaaaa", "aaaaaaaab", "aaaaaaaaab"});
    test_insert(art, {"aaaaaaaaaa"}, {"aaaaaaaaa"}, {"aaaaaaaab", "aaaaaaaaab"});
    test_insert(art, {"aaaaaaaab"}, {"aaaaaaaaa", "aaaaaaaaaa"}, {"aaaaaaaaab"});
    test_insert(art, {"aaaaaaaaab"}, {"aaaaaaaaa", "aaaaaaaaaa", "aaaaaaaab"}, {});

    test_erase(art, {"aaaaaaaaab"}, {"aaaaaaaaa", "aaaaaaaaaa", "aaaaaaaab"}, {});
    test_erase(art, {"aaaaaaaab"}, {"aaaaaaaaa", "aaaaaaaaaa"}, {"aaaaaaaaab"});
    test_erase(art, {"aaaaaaaaaa"}, {"aaaaaaaaa"}, {"aaaaaaaab", "aaaaaaaaab"});
    test_erase(art, {"aaaaaaaaa"}, {}, {"aaaaaaaaaa", "aaaaaaaab", "aaaaaaaaab"});
}

TEST(art_tests, medium_size_keys_insertion)
{
    ART art;

    test_insert(art, {"abcdefghijklmnopqrstuvwxyz"}, {},
                {"abcdefghijklmnopqrstuvwxy", "abcdefghijklmnopqrstuvwxyzz"});

    test_insert(art, {"abcdefghijklmnopqrstuvwxy"}, {"abcdefghijklmnopqrstuvwxyz"},
                {"abcdefghijklmnopqrstuvwxyzz"});

    test_insert(art, {"abcdefghijklmnopqrstuvwxyzz"},
                {"abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxy"}, {});

    test_erase(art, {"abcdefghijklmnopqrstuvwxyzz"},
               {"abcdefghijklmnopqrstuvwxyz", "abcdefghijklmnopqrstuvwxy"}, {});

    test_erase(art, {"abcdefghijklmnopqrstuvwxy"}, {"abcdefghijklmnopqrstuvwxyz"},
               {"abcdefghijklmnopqrstuvwxyzz"});

    test_erase(art, {"abcdefghijklmnopqrstuvwxyz"}, {},
               {"abcdefghijklmnopqrstuvwxy", "abcdefghijklmnopqrstuvwxyzz"});
}

TEST(art_tests, long_keys_insertion)
{
    ART art;

    constexpr size_t str_len = 1024ULL * 1024;
    const std::string long_str(str_len, '!');

    art.insert(long_str);
    assert_search(art, long_str);
    assert_failed_search(art, "a" + long_str);

    art.insert("a" + long_str);
    art.insert("b" + long_str);
    art.insert("c" + long_str);
    art.insert("d" + long_str);
    art.insert("e" + long_str);

    assert_search(art, long_str);
    assert_search(art, "a" + long_str);
    assert_search(art, "b" + long_str);
    assert_search(art, "c" + long_str);
    assert_search(art, "d" + long_str);
    assert_search(art, "e" + long_str);
    assert_failed_search(art, "f" + long_str);
}

TEST(art_tests, growing_nodes)
{
    ART art;

    constexpr size_t str_len = 1024ULL;
    const std::string long_str(str_len, '!');

    std::vector<std::string> keys;

    for (int i = 1; i < 256; ++i)
        keys.push_back(char(i) + long_str);

    test_insert(art, keys, {}, {});
    test_erase(art, keys, {}, {});
}

TEST(art_tests, growing_nodes_2)
{
    ART art;

    constexpr size_t str_len = 8ULL;
    const std::string long_str(str_len, '!');

    std::vector<std::string> keys;

    const size_t buf_size = 16;
    char buf[buf_size];
    std::memset(buf, 1, buf_size);

    keys.push_back(std::string(buf, buf + buf_size));
    keys.push_back(std::string(buf, buf + buf_size) + long_str);
    keys.push_back(long_str + std::string(buf, buf + buf_size));

    for (int i = 0; i < buf_size; ++i) {
        for (int j = 2; j < 9; ++j) {
            buf[i] = j;
            keys.push_back(std::string(buf, buf + buf_size));
            keys.push_back(std::string(buf, buf + buf_size) + long_str);
            keys.push_back(long_str + std::string(buf, buf + buf_size));
        }
    }

    test_insert(art, keys, {}, {});
    test_erase(art, keys, {}, {});
}

TEST(art_tests, growing_nodes_3)
{
    ART art;

    constexpr size_t key_size = 1024;
    std::vector<uint8_t> key(key_size, 1); // Fill vector with 1s.

    for (int i = 0; i < key_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            key[i] = j;
            art.insert((const uint8_t*)key.data(), key_size);
        }
    }

    for (auto& it : key)
        it = 1;

    for (int i = 0; i < key_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            key[i] = j;
            assert_search(art, (const uint8_t*)key.data(), key_size);
        }
    }
}

TEST(art_tests, different_key_sizes)
{
    ART art;

    constexpr size_t key_max_size = 1024;
    std::vector<uint8_t> v(key_max_size, 1); // Fill vector with 1s.

    for (int i = 0; i < key_max_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            v[i] = j;
            art.insert((const uint8_t*)v.data(), i + 1);
        }
    }

    for (auto& it : v)
        it = 1;

    for (int i = 0; i < key_max_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            v[i] = j;
            assert_search(art, (const uint8_t*)v.data(), i + 1);
        }
    }
}

// Reads all filesystem paths from provided input file, inserts them into ART and search for them 1
// by 1 while verifying searches.
//
void test_filesystem_paths(const std::string& file_name)
{
    ART art;

    std::ifstream in_file_stream{"test/input_files/" + file_name};
    std::vector<std::string> paths;

    for (std::string file_path; std::getline(in_file_stream, file_path);) {
        paths.push_back(file_path);
        art.insert(paths.back());
    }

    for (auto& it : paths)
        assert_search(art, it);
}

TEST(art_tests, file_system_paths)
{
    std::vector<std::string> file_names{
        "windows_paths.txt",
        "linux_paths.txt",
        "windows_paths_vscode.txt",
    };

    for (const auto& file_name : file_names)
        test_filesystem_paths(file_name);
}

// NOLINTEND
