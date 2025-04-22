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
#include "files.h"
#include "fs_trie.h"
#include "symbol_finder.h"

// NOLINTBEGIN

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
    std::string input;
    std::string root;
    std::string options;
    std::string regex;

    std::cout << std::format("Options: <root_dir> <-fs>\n: ");
    std::getline(std::cin, input);

    std::stringstream ss{input};
    ss >> root, ss >> options;

    uint32_t i32_opt = 0;
    if (options.find('f') != std::string::npos)
        i32_opt |= Options::files;
    if (options.find('s') != std::string::npos)
        i32_opt |= Options::symbols;

    Symbol_finder finder{std::move(root), Options{i32_opt}};
    finder.print_memory_usage();

    while (true) {
        std::cout << ": ";
        std::getline(std::cin, input);
        std::stringstream ss{input};
        ss >> options;
        ss >> regex;

        if (regex.empty()) {
            std::cout << "Invalid search options. Find with <-fs> <regex>\n";
            continue;
        }

        i32_opt = 0;
        if (options.find('f') != std::string::npos)
            i32_opt |= Options::files;
        if (options.find('s') != std::string::npos)
            i32_opt |= Options::symbols;

        Options opt{i32_opt};

        if (opt.files_allowed())
            finder.find_files(regex);

        if (opt.symbols_allowed())
            finder.find_symbols(regex);
    }

    // art::ART art_full;
    // art::ART art_files_only;

    // std::string dir = "C:\\";
    // using dir_iter = fs::recursive_directory_iterator;
    // constexpr auto it_opt = fs::directory_options::skip_permission_denied;

    // std::error_code ec;
    // for (dir_iter it(dir, it_opt, ec); it != dir_iter(); it.increment(ec)) {
    //     if (!check_iteration(it, ec))
    //         continue;

    //     std::string s = it->path().string();
    //     art_full.insert(it->path().string());
    //     art_files_only.insert(it->path().filename().string());

    //     // std::reverse(s.begin(), s.end());
    //     // art.insert(s);
    // }

    // std::cout << "ART full size: " << art_full.size_in_bytes() / 1024 / 1024 << "MB\n";
    // std::cout << "ART full leaves count: " << art_full.leaves_count() << "\n";

    // std::cout << "ART files only size: " << art_files_only.size_in_bytes() / 1024 / 1024 <<
    // "MB\n"; std::cout << "ART files only leaves count: " << art_files_only.leaves_count() <<
    // "\n";

    // std::cin >> dir;
}

// NOLINTEND
