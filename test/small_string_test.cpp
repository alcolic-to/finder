#include <gtest/gtest.h>

#include "small_string.h"

// NOLINTBEGIN

TEST(small_string_test, sanity_test_1)
{
    SmallString s0{""};
    SmallString s1{"12345"};
    SmallString s2{"123456"};
    SmallString s3{"1234567"};
    SmallString s4{"12345678"};

    ASSERT_TRUE(s0 == std::string{""});
    ASSERT_TRUE(s1 == std::string{"12345"});
    ASSERT_TRUE(s2 == std::string{"123456"});
    ASSERT_TRUE(s3 == std::string{"1234567"});
    ASSERT_TRUE(s4 == std::string{"12345678"});

    SmallString s0_copy{s0};
    SmallString s1_copy{s1};

    ASSERT_TRUE(s0 == s0_copy);
    ASSERT_TRUE(s1 == s1_copy);

    SmallString s2_move{std::move(s2)};
    ASSERT_TRUE(s2 != s2_move);
    ASSERT_TRUE(s2 != std::string("123456"));
    ASSERT_TRUE(s2_move == std::string("123456"));

    // std::cout << s0 << "\n";
    // std::cout << s1 << "\n";
    // std::cout << s2 << "\n";
    // std::cout << s3 << "\n";
    // std::cout << s4 << "\n";
}

// NOLINTEND
