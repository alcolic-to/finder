#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string_view>
#include <vector>

#include "trie.h"

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

std::string file_to_string(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::string{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

std::vector<char> file_to_vector(const std::string& path)
{
    std::ifstream f{path, std::ios_base::binary};
    return std::vector<char>{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
}

int main(int argc, char* argv[])
{
    // std::cout << "Hello, new cpp project.\n" << __FILE__;

    // Stopwatch<true, microseconds> s;
    //
    // std::vector<char> file{file_to_vector(__FILE__)};
    // std::cout << "Done. " << file[0] << "\n";

    Trie trie{"banana"};
    trie.insert("ananas");

    auto* node = trie.find("n");

    if (node != nullptr)
        for (const auto& r : node->all_results())
            std::cout << r << "\n";
    else
        std::cout << "Not found\n";
}