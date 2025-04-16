#include <filesystem>
#include <ranges>
#include <vector>

#include "suffix_trie.h"

#ifndef FILES_H
#define FILES_H

// NOLINTBEGIN

namespace fs = std::filesystem;

class File_info {
public:
    File_info() = default;

    File_info(std::string file_name) : m_name{std::move(file_name)} {}

    File_info(std::string file_name, std::string_view file_path, size_t size)
        : m_name{std::move(file_name)}
        , m_path{std::move(file_path)}
        , m_size{size}
    {
        if (!file_path.ends_with(file_name))
            throw std::runtime_error{"File path does not end with file name."};
    }

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }

    [[nodiscard]] const std::string_view path() const noexcept { return m_path; }

    void set_path(std::string_view path) { m_path = path; }

private:
    std::string m_name;      // File name with extension.
    std::string_view m_path; // Full file path.
    size_t m_size = 0;
};

// Class that holds all file system files, their paths, size infos, etc.
// TODO: Files can be found by regex search.
//
class Files {
public:
    void insert(const fs::path& path) { insert(path.filename().string(), path.string()); }

    void insert(std::string file_name, std::string file_path)
    {
        // if (m_files.search(file_path) != nullptr)
        //    return;

        // TODO: Try inserting file_path with cutted file name from it and see how much space can we
        // save.
        auto res = m_files.insert(file_path, File_info{file_name});
        if (!res)
            return;

        File_info* file = &res->value();

        file->set_path(res->key_to_string_view());
        m_files_refs.push_back(file);

        // TODO: Create emplace to avoid this nonsence.
        auto ff_res = m_file_finder.insert_suffix(file->name(), std::vector{file});
        if (ff_res)
            return;

        auto& file_infos = *ff_res->value();
        assert(std::ranges::find(file_infos, file) == file_infos.end());
        file_infos.emplace_back(file);
    }

    void erase(const fs::path& path) { erase(path.filename().string(), path.string()); }

    void erase(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_files.search(file_path);
        if (res == nullptr)
            return;

        File_info* file = &res->value();

        // Erase file suffixes.
        //
        auto ff_res = m_file_finder.search(file->name());
        assert(ff_res);

        auto& file_infos = *ff_res->value().value();

        auto fi_it = std::ranges::find(file_infos, file);
        assert(fi_it != file_infos.end());

        file_infos.erase(fi_it);
        if (file_infos.empty())
            m_file_finder.erase_suffix(file->name());

        // Remove file from references vector.
        //
        auto vit = std::ranges::find(m_files_refs, file);
        assert(vit != m_files_refs.end());
        m_files_refs.erase(vit);

        // Erase file from files trie.
        //
        m_files.erase(file_path);
    }

    auto search(const std::string& regex)
    {
        std::vector<const File_info*> results;

        auto info_vectors = m_file_finder.search_prefix(regex);
        for (auto info_vector : info_vectors)
            for (auto file_info : *info_vector)
                results.push_back(file_info);

        return results;
    }

    auto files_size(bool full_leaves = true) { return m_files.size_in_bytes(full_leaves); }

    auto file_finder_size(bool full_leaves = true)
    {
        return m_file_finder.size_in_bytes(full_leaves);
    }

    auto files_refs_size() { return m_files_refs.size() * sizeof(File_info*); }

private:
    // Trie that will hold files where key is full file path.
    ///
    art::ART<File_info> m_files;

    // Vector of file pointers for quick reach of all files.
    //
    std::vector<File_info*> m_files_refs;

    // Trie that holds all suffixes of all files, which enables us file search by file name.
    //
    suffix_trie::Suffix_trie<std::vector<File_info*>> m_file_finder;
};

// NOLINTEND

#endif // FILES_H
