
#include <filesystem>
#include <ranges>
#include <vector>

#include "suffix_trie.h"

#ifndef FS_TRIE_H
#define FS_TRIE_H

// NOLINTBEGIN

namespace fs = std::filesystem;

// Trie that holds filesystem files and their paths.
// This is just an example of how suffix trie can be used.
//
class FS_trie : public suffix_trie::Suffix_trie<std::vector<std::string>> {
public:
    using STrie = suffix_trie::Suffix_trie<std::vector<std::string>>;

    void insert_file_path(const fs::path& path)
    {
        insert_file_path(path.filename().string(), path.string());
    }

    void insert_file_path(std::string file_name, std::string file_path)
    {
        auto res = STrie::insert_suffix(file_name, std::vector{file_path});
        if (res)
            return;

        // Filename already exist in a suffix trie, so just add path.
        //
        auto& paths = *res->value().m_value;
        if (std::ranges::find(paths, file_path) == paths.end())
            paths.emplace_back(std::move(file_path));
    }

    void erase_file_path(const fs::path& path)
    {
        erase_file_path(path.filename().string(), path.string());
    }

    void erase_file_path(const std::string& file_name, const std::string& file_path)
    {
        auto res = STrie::search(file_name);
        if (!res)
            return;

        auto& file_paths = *res->value().m_value;

        // Check if file path exist in a paths vector.
        //
        auto file_path_it = std::ranges::find(file_paths, file_path);
        if (file_path_it == file_paths.end())
            return;

        // Erase filepath and delete all suffixes if there are no file paths.
        //
        file_paths.erase(file_path_it);
        if (file_paths.empty())
            STrie::erase_suffix(file_name);
    }

    auto search(const std::string& regex)
    {
        std::vector<const std::string*> results;

        auto ptrs = STrie::search_prefix(regex);
        for (auto str_vector : ptrs)
            for (const auto& str : *str_vector)
                results.push_back(&str);

        return results;
    }
};

// NOLINTEND

#endif
