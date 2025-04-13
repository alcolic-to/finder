#include <chrono>
#include <gtest/gtest.h>
#include <iterator>
#include <string>

#include "art.h"

// NOLINTBEGIN

using namespace std::chrono;
using namespace std::chrono_literals;
using Clock = steady_clock;
using Time_point = std::chrono::time_point<Clock>;

inline Time_point now() noexcept
{
    return Clock::now();
}

template<bool print = true, typename Unit = milliseconds>
class Stopwatch {
public:
    explicit Stopwatch(std::string name = "Stopwatch") noexcept
        : m_name{std::move(name)}
        , m_start{now()}
    {
    }

    ~Stopwatch() noexcept
    {
        if constexpr (print) {
            auto out = elapsed_units().count();
            std::cout << m_name << " elapsed time: " << out << " " << unit_name() << "\n";
        }
    }

    Stopwatch(const Stopwatch& rhs) = delete;
    Stopwatch& operator=(const Stopwatch& rhs) = delete;
    Stopwatch(Stopwatch&& rhs) noexcept = delete;
    Stopwatch& operator=(Stopwatch&& rhs) = delete;

    void restart() noexcept { m_start = now(); }

    [[nodiscard]] auto elapsed() const noexcept { return now() - m_start; }

    [[nodiscard]] auto elapsed_units() const noexcept { return duration_cast<Unit>(elapsed()); }

    [[nodiscard]] std::string unit_name() const noexcept
    {
        // clang-format off
        if      constexpr (std::is_same_v<Unit, hours>)        return "hour(s)";
        else if constexpr (std::is_same_v<Unit, minutes>)      return "minute(s)";
        else if constexpr (std::is_same_v<Unit, seconds>)      return "second(s)";
        else if constexpr (std::is_same_v<Unit, milliseconds>) return "millisecond(s)";
        else if constexpr (std::is_same_v<Unit, microseconds>) return "microsecond(s)";
        else if constexpr (std::is_same_v<Unit, nanoseconds>)  return "nanosecond(s)";
        else                                                   return "unknown unit";
        // clang-format on
    }

private:
    std::string m_name;
    Clock::time_point m_start;
};

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
