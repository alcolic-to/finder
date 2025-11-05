#include <cstddef>
#include <gtest/gtest.h>

#include "array_map.h"
#include "test_util.h"
#include "util.h"

// NOLINTBEGIN

TEST(array_map_tests, sanity_test)
{
    ArrayMap<int> arr;

    arr.insert(0, 0);

    ASSERT_TRUE(!arr.empty());

    arr.insert(1, 1);
    arr.insert(2, 2);
    arr.insert(3, 3);
    arr.insert(4, 4);

    ASSERT_TRUE(arr[0] == 0);
    ASSERT_TRUE(arr[1] == 1);
    ASSERT_TRUE(arr[2] == 2);
    ASSERT_TRUE(arr[3] == 3);
    ASSERT_TRUE(arr[4] == 4);

    arr.insert(5, 9);
    arr.insert(6, 8);
    arr.insert(7, 7);
    arr.insert(8, 6);
    arr.insert(9, 5);

    ASSERT_TRUE(arr[5] == 9);
    ASSERT_TRUE(arr[6] == 8);
    ASSERT_TRUE(arr[7] == 7);
    ASSERT_TRUE(arr[8] == 6);
    ASSERT_TRUE(arr[9] == 5);

    ASSERT_TRUE(arr.contains(0));
    arr.erase(0);
    ASSERT_TRUE(!arr.contains(0));

    ASSERT_TRUE(arr.contains(1));
    arr.erase(1);
    ASSERT_TRUE(!arr.contains(1));

    ASSERT_TRUE(arr.contains(2));
    arr.erase(2);
    ASSERT_TRUE(!arr.contains(2));

    ASSERT_TRUE(arr.contains(3));
    arr.erase(3);
    ASSERT_TRUE(!arr.contains(3));

    ASSERT_TRUE(arr.contains(4));
    arr.erase(4);
    ASSERT_TRUE(!arr.contains(4));

    ASSERT_TRUE(arr.contains(5));
    arr.erase(5);
    ASSERT_TRUE(!arr.contains(5));

    ASSERT_TRUE(arr.contains(6));
    arr.erase(6);
    ASSERT_TRUE(!arr.contains(6));

    ASSERT_TRUE(arr.contains(7));
    arr.erase(7);
    ASSERT_TRUE(!arr.contains(7));

    ASSERT_TRUE(arr.contains(8));
    arr.erase(8);
    ASSERT_TRUE(!arr.contains(8));

    ASSERT_TRUE(arr.contains(9));
    arr.erase(9);
    ASSERT_TRUE(!arr.contains(9));

    ASSERT_TRUE(arr.empty());
}

TEST(array_map_tests, types_test)
{
    ArrayMap<std::string> arr;

    arr.insert(0, "I am 0 entry.");

    ASSERT_TRUE(!arr.empty());

    arr.insert(1, "I am 1 entry.");
    arr.insert(2, "I am 2 entry.");
    arr.insert(3, "I am 3 entry.");
    arr.insert(4, "I am 4 entry.");

    ASSERT_TRUE(arr[0] == "I am 0 entry.");
    ASSERT_TRUE(arr[1] == "I am 1 entry.");
    ASSERT_TRUE(arr[2] == "I am 2 entry.");
    ASSERT_TRUE(arr[3] == "I am 3 entry.");
    ASSERT_TRUE(arr[4] == "I am 4 entry.");

    arr[3] = "I am 10 entry";
    ASSERT_TRUE(arr[3] == "I am 10 entry");

    ASSERT_TRUE(arr.contains(0));
    arr.erase(0);
    ASSERT_TRUE(!arr.contains(0));

    ASSERT_TRUE(arr.contains(1));
    arr.erase(1);
    ASSERT_TRUE(!arr.contains(1));

    ASSERT_TRUE(arr.contains(2));
    arr.erase(2);
    ASSERT_TRUE(!arr.contains(2));

    ASSERT_TRUE(arr.contains(3));
    arr.erase(3);
    ASSERT_TRUE(!arr.contains(3));

    ASSERT_TRUE(arr.contains(4));
    arr.erase(4);
    ASSERT_TRUE(!arr.contains(4));

    ASSERT_TRUE(arr.empty());
}

// NOLINTEND