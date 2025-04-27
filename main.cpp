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
#include "suffix_trie.h"
#include "symbol_finder.h"

// NOLINTBEGIN

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
    // std::string file_names_string;
    // std::string file_names_suffixes_string;
    // std::string file_paths_string;
    // art::ART files_suffixes;
    // // suffix_trie::Suffix_trie_2 files_suffixes;

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

    //     file_paths_string += it->path().string();
    //     file_names_string += it->path().filename().string();

    //     std::string file_name = it->path().filename().string();

    //     uint8_t* begin = (uint8_t*)file_name.data();
    //     uint8_t* end = begin + file_name.size();

    //     while (begin <= end) {
    //         file_names_suffixes_string += std::string(begin, end);
    //         files_suffixes.insert(begin, end - begin);

    //         ++begin;
    //     }

    //     // files_suffixes.insert_suffix(it->path().filename().string());

    //     // std::reverse(s.begin(), s.end());
    //     // art.insert(s);
    // }

    // std::cout << file_names_string << "\n";

    // std::cout << "File name suffixes size: " << files_suffixes.size_in_bytes() << "\n";
    // std::cout << "File name suffixes string size: " << file_names_suffixes_string.size() << "\n";

    // std::cout << "File path string size: " << file_paths_string.size() << "\n";
    // std::cout << "File name string size: " << file_names_string.size() << "\n";

    // std::cout << "ART full size: " << art_full.size_in_bytes() / 1024 / 1024 << "MB\n";
    // std::cout << "ART full nodes count: " << art_full.nodes_count() << "\n";
    // std::cout << "ART full leaves count: " << art_full.leaves_count() << "\n";

    // std::cout << "ART files only size: " << art_files_only.size_in_bytes() / 1024 / 1024 <<
    // "MB\n"; std::cout << "ART files only nodes count: " << art_files_only.nodes_count() << "\n";
    // std::cout << "ART files only leaves count: " << art_files_only.leaves_count() << "\n";

    // // std::cout << "File name suffixes size: " << files_suffixes.size_in_bytes() << "\n";

    // std::cin >> dir;
}

// NOLINTEND
