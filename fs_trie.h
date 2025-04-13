
#include <filesystem>
#include <initializer_list>
#include <map>
#include <ranges>
#include <unordered_map>
#include <vector>

#include "suffix_trie.h"

// NOLINTBEGIN

namespace fs = std::filesystem;

// Trie that holds filesystem files, file paths, and another files informations.
//
class FS_trie {
public:
    void insert_file_path(const fs::path& path)
    {
        insert_file_path(path.filename().string(), path.string());
    }

    void insert_file_path(std::string filename, std::string filepath)
    {
        auto res = m_trie.insert_suffix(std::move(filename), 1, filepath);
        if (res)
            return;

        // Filename already exist in a suffix trie, so just add path.
        //
        auto& paths = res.get()->second;
        if (std::ranges::find(paths, filepath) == paths.end())
            paths.emplace_back(std::move(filepath));
    }

    void erase_file_path(const fs::path& path)
    {
        erase_file_path(path.filename().string(), path.string());
    }

    void erase_file_path(const std::string& filename, const std::string& filepath)
    {
        // Check if filename exists in a trie.
        //
        auto f_it = m_trie.$().find(filename);
        if (f_it == m_trie.$().end())
            return;

        auto& filepaths = f_it->second;

        // Check if file path exist in a paths vector.
        //
        auto filepath_it = std::ranges::find(filepaths, filepath);
        if (filepath_it == filepaths.end())
            return;

        // Erase filepath.
        //
        filepaths.erase(filepath_it);
        if (filepaths.empty())
            m_trie.erase_suffix(filename);
    }

    auto search(const std::string& regex) { return m_trie.search_prefix(regex); }

    const auto $() const noexcept { return m_trie.$(); }

    auto $() noexcept { return m_trie.$(); }

private:
    suffix_trie::Suffix_trie<std::vector<std::string>> m_trie;
};

// NOLINTEND