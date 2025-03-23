#include <cstddef>
#include <gtest/gtest.h>

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

TEST(art_tests, sanity_test)
{
    ART art;

    art.insert("a");

    assert_search(art, "a");

    assert_failed_search(art, "");
    assert_failed_search(art, "aa");
    assert_failed_search(art, "b");

    art.insert("");

    assert_search(art, "");

    assert_failed_search(art, "aa");
    assert_failed_search(art, "b");
}

TEST(art_tests, multiple_insertions)
{
    ART art;

    art.insert("abcdef");
    art.insert("abcde");
    art.insert("a");
    art.insert("abcdefgh");

    assert_search(art, "abcdefgh");
    assert_search(art, "a");
    assert_search(art, "abcde");
    assert_search(art, "abcdefgh");

    assert_failed_search(art, "abcdefghi");
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

TEST(art_tests, similar_keys_insertion_2)
{
    ART art;

    art.insert("a");
    art.insert("aa");
    art.insert("aaa");
    art.insert("aaaa");
    art.insert("aaaaa");
    art.insert("aaaaaa");
    art.insert("aaaaaaa");

    assert_search(art, "a");
    assert_search(art, "aa");
    assert_search(art, "aaa");
    assert_search(art, "aaaa");
    assert_search(art, "aaaaa");
    assert_search(art, "aaaaaa");
    assert_search(art, "aaaaaaa");

    assert_failed_search(art, "");
    assert_failed_search(art, "aaaaaaaa");
    assert_failed_search(art, "b");
    assert_failed_search(art, "ab");
    assert_failed_search(art, "aab");
    assert_failed_search(art, "aaab");
    assert_failed_search(art, "aaaab");
    assert_failed_search(art, "aaaaab");
    assert_failed_search(art, "aaaaaab");
    assert_failed_search(art, "aaaaaaab");
}

TEST(art_tests, similar_prefix_insertions)
{
    ART art;

    art.insert("aaaaaaaaa");

    assert_search(art, "aaaaaaaaa");
    assert_failed_search(art, "aaaaaaaaaa");
    assert_failed_search(art, "aaaaaaaab");
    assert_failed_search(art, "aaaaaaaaab");

    art.insert("aaaaaaaaaa");

    assert_search(art, "aaaaaaaaa");
    assert_search(art, "aaaaaaaaaa");
    assert_failed_search(art, "aaaaaaaab");
    assert_failed_search(art, "aaaaaaaaab");

    art.insert("aaaaaaaab");

    assert_search(art, "aaaaaaaaa");
    assert_search(art, "aaaaaaaaaa");
    assert_search(art, "aaaaaaaab");
    assert_failed_search(art, "aaaaaaaaab");

    art.insert("aaaaaaaaab");

    assert_search(art, "aaaaaaaaa");
    assert_search(art, "aaaaaaaaaa");
    assert_search(art, "aaaaaaaab");
    assert_search(art, "aaaaaaaaab");
}

TEST(art_tests, medium_sizes_keys_insertion)
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

// NOLINTEND
