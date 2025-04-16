#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "art.h"
#include "fs_trie.h"
// #include "symbol_finder.h"
#include "files.h"
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

// Taken from google benchmark.
//
// NOLINTBEGIN
#if defined(__GNUC__) || defined(__clang__)
#define ALWAYS_INLINE __attribute__((always_inline))
#elif defined(_MSC_VER) && !defined(__clang__)
#define ALWAYS_INLINE __forceinline
#define __func__ __FUNCTION__
#else
#define ALWAYS_INLINE
#endif

// Taken from google benchmark.
//
template<class Tp>
inline ALWAYS_INLINE void dont_optimize(Tp&& value)
{
#if defined(__clang__)
    asm volatile("" : "+r,m"(value) : : "memory");
#else
    asm volatile("" : "+m,r"(value) : : "memory");
#endif
}

// NOLINTEND

int main(int argc, char* argv[])
{
    try {
        Files files;
        std::string path{R"(C:\Users\topac\.vscode)"};

        {
            Stopwatch s{"Scanning"};
            for (const auto& dir_entry : std::filesystem::recursive_directory_iterator{path})
                files.insert(dir_entry);
        }

        std::cout << "Entries count: " << files.search("").size() << "\n";

        std::string str_for_match;
        while (true) {
            std::cout << ": ";
            std::cin >> str_for_match;

            // std::unordered_set<std::string_view> results;
            auto res = [&] {
                Stopwatch<true, microseconds> s{"Search"};
                return files.search(str_for_match);
            }();

            for (const auto* file_info : res)
                std::cout << file_info->path() << "\n";

            std::cout << "Results size: " << res.size() << "\n";
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {
        std::cout << "Exception: std::filesystem::filesystem_error " << ex.what() << "\n";
        std::cout << "Exception: " << ex.code() << "\n";
        std::cout << "Exception: " << ex.path2() << "\n";
        std::cout << "Exception: " << ex.path1() << "\n";
    }
    catch (const std::exception& ex) {
        std::cout << "Exception std::exception: " << ex.what() << "\n";
    }
}
