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

    // art.insert("a");

    // assert_search(art, "a");

    // assert_failed_search(art, "");
    // assert_failed_search(art, "aa");
    // assert_failed_search(art, "b");

    // art.insert("");

    // assert_search(art, "");

    // assert_failed_search(art, "aa");
    // assert_failed_search(art, "b");

    // art.erase("a");

    // assert_search(art, "");

    // assert_failed_search(art, "a");
    // assert_failed_search(art, "aa");
    // assert_failed_search(art, "b");

    // art.erase("");

    // assert_failed_search(art, "");
    // assert_failed_search(art, "a");
    // assert_failed_search(art, "aa");
    // assert_failed_search(art, "b");

    // New code:
    //
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

    // art.insert("aaaaaaaaa");

    // assert_search(art, "aaaaaaaaa");
    // assert_failed_search(art, "aaaaaaaaaa");
    // assert_failed_search(art, "aaaaaaaab");
    // assert_failed_search(art, "aaaaaaaaab");

    // art.insert("aaaaaaaaaa");

    // assert_search(art, "aaaaaaaaa");
    // assert_search(art, "aaaaaaaaaa");
    // assert_failed_search(art, "aaaaaaaab");
    // assert_failed_search(art, "aaaaaaaaab");

    // art.insert("aaaaaaaab");

    // assert_search(art, "aaaaaaaaa");
    // assert_search(art, "aaaaaaaaaa");
    // assert_search(art, "aaaaaaaab");
    // assert_failed_search(art, "aaaaaaaaab");

    // art.insert("aaaaaaaaab");

    // assert_search(art, "aaaaaaaaa");
    // assert_search(art, "aaaaaaaaaa");
    // assert_search(art, "aaaaaaaab");
    // assert_search(art, "aaaaaaaaab");

    // New code:
    //
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

    art.insert("abcdefghijklmnopqrstuvwxyz");

    assert_search(art, "abcdefghijklmnopqrstuvwxyz");

    assert_failed_search(art, "abcdefghijklmnopqrstuvwxy");
    assert_failed_search(art, "abcdefghijklmnopqrstuvwxyzz");

    art.insert("abcdefghijklmnopqrstuvwxy");

    assert_search(art, "abcdefghijklmnopqrstuvwxyz");
    assert_search(art, "abcdefghijklmnopqrstuvwxy");

    assert_failed_search(art, "abcdefghijklmnopqrstuvwxyzz");

    art.insert("abcdefghijklmnopqrstuvwxyzz");

    assert_search(art, "abcdefghijklmnopqrstuvwxyz");
    assert_search(art, "abcdefghijklmnopqrstuvwxy");
    assert_search(art, "abcdefghijklmnopqrstuvwxyzz");
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

    constexpr size_t str_len = 1024ULL * 1024;
    const std::string long_str(str_len, '!');

    // Grow to 256 node.
    //
    for (int i = 1; i < 256; ++i)
        art.insert(char(i) + long_str);

    for (int i = 1; i < 256; ++i)
        assert_search(art, char(i) + long_str);
}

TEST(art_tests, growing_nodes_2)
{
    ART art;

    constexpr size_t str_len = 1024ULL;
    const std::string long_str(str_len, '!');

    char buf[256]{};
    std::memset(buf, 1, 256);

    for (int i = 0; i < 256; ++i) {
        for (int j = 1; j < 256; ++j) {
            buf[i] = j;
            art.insert((const uint8_t*)buf, 256);
            art.insert(std::string(buf, buf + 255) + long_str);
            art.insert(long_str + std::string(buf, buf + 255));
        }
    }

    std::memset(buf, 1, 256);

    for (int i = 0; i < 256; ++i) {
        for (int j = 1; j < 256; ++j) {
            buf[i] = j;
            assert_search(art, (const uint8_t*)buf, 256);
            assert_search(art, std::string(buf, buf + 255) + long_str);
            assert_search(art, long_str + std::string(buf, buf + 255));
        }
    }
}

TEST(art_tests, growing_nodes_3)
{
    ART art;

    constexpr size_t key_size = 1024;
    std::vector<uint8_t> v(key_size, 1); // Fill vector with 1s.

    for (int i = 0; i < key_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            v[i] = j;
            art.insert((const uint8_t*)v.data(), key_size);
        }
    }

    for (auto& it : v)
        it = 1;

    for (int i = 0; i < key_size; ++i) {
        for (int j = 1; j < 256; ++j) {
            v[i] = j;
            assert_search(art, (const uint8_t*)v.data(), key_size);
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
