#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <ranges>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "art.h"
#include "fs_trie.h"
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

using namespace suffix_trie;

// NOLINTEND

int main(int argc, char* argv[])
{
    // Suffix_trie trie;

    // for (std::string s{"Aleksandar"}; !s.empty(); s = s.substr(0))
    //     ;
    // trie.
    // ART<int> art;

    // art.insert("Aleksandar");

    // std::string s{"banana"};
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "ana";
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "anana";
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "ananas";
    // art.insert((uint8_t*)s.data(), s.size());

    // std::string s{"aaaaaa"};
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "aaaaa";
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "a";
    // art.insert((uint8_t*)s.data(), s.size());

    // s = "aaaaaaaa";
    // art.insert((uint8_t*)s.data(), s.size());

    // Leaf* l = art.search((uint8_t*)s.data(), s.size());
    // std::cout << l->m_key << "\n";

    // s = "a", l = art.search((uint8_t*)s.data(), s.size());
    // std::cout << l->m_key << "\n";

    // s = "aaaaa", l = art.search((uint8_t*)s.data(), s.size());
    // std::cout << l->m_key << "\n";

    // s = "aaaaaa", l = art.search((uint8_t*)s.data(), s.size());
    // std::cout << l->m_key << "\n";

    // std::cout << "Added tests and benchmark.\n";

    // std::cout << "Hello, new cpp project.\n" << __FILE__;

    // Stopwatch<true, microseconds> s;
    //
    // std::vector<char> file{file_to_vector(__FILE__)};
    // std::cout << "Done. " << file[0] << "\n";

    // Trie trie;
    // trie.insert("banana");
    // trie.insert("BANANA");
    // trie.insert("ananas");

    // for (const auto& it : trie.find<Options::icase>("ban"))
    //     std::cout << it << "\n";

    // trie.delete_suffix("ananas");
    // trie.delete_suffix("banana");

    // auto* node = trie.find("n");

    // if (node != nullptr)
    //     for (const auto& r : node->all_results())
    //         std::cout << r << "\n";
    // else
    //     std::cout << "Not found\n";

    try {
        FS_trie trie;
        std::string path{R"(C:\Users\topac\.vscode)"};
        for (const auto& dir_entry : std::filesystem::recursive_directory_iterator{path}) {
            // if (dir_entry.is_regular_file()) {
            //     std::vector v{file_to_vector(dir_entry.path().string())};

            //     std::cout << dir_entry.path().string() << ": " << v[v.size() - 5] << "\n";
            // }

            trie.insert_file_path(dir_entry.path().filename().string(), dir_entry.path().string());
        }

        std::cout << "All $s: " << trie.$().size() << "\n";

        std::string str_for_match;

        {
            while (true) {
                std::cin >> str_for_match;

                // std::unordered_set<std::string_view> results;
                auto r = [&] {
                    Stopwatch<true, microseconds> s;
                    return trie.search(str_for_match);
                }();

                for (auto& entry : r) {
                    // std::cout << "File name: " << entry->first << "\n";
                    for (auto& path : entry->second)
                        std::cout << path << "\n";
                }

                // {
                //     Stopwatch<true, microseconds> s;
                //     results = trie.find<Options::icase>(str_for_match);
                // }

                // dont_optimize(results);
                // for (const auto& r : results)
                //     std::cout << r << "\n";

                // std::ranges::sort(results);
                // for (const auto& r : results)
                //     std::cout << r << "\n";

                // std::cout << "Results size: " << results.size() << "\n";
                // std::cout << "Results size: "
                //           << std::unordered_set(results.begin(), results.end()).size() <<
                //           "\n";
            }
        }
    }
    catch (const std::filesystem::filesystem_error& ex) {
        std::cout << "Exception: std::filesystem::filesystem_error" << ex.what() << "\n";
        std::cout << "Exception: " << ex.code() << "\n";
        std::cout << "Exception: " << ex.path2() << "\n";
        std::cout << "Exception: " << ex.path1() << "\n";
    }
    catch (const std::exception& ex) {
        std::cout << "Exception std::exception: " << ex.what() << "\n";
    }
}
