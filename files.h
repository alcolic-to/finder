#include <filesystem>
#include <memory>
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
    void insert(const fs::path& path)
    {
        insert(path.filename().string(), path.parent_path().string());
    }

    void erase(const fs::path& path)
    {
        erase(path.filename().string(), path.parent_path().string());
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

    auto files_size()
    {
        return m_files.size() * (sizeof(File_info) + sizeof(std::unique_ptr<File_info>));
    }

    auto file_paths_size(bool full_leaves = true)
    {
        return m_file_paths.size_in_bytes(full_leaves);
    }

    auto file_finder_size(bool full_leaves = true)
    {
        return m_file_finder.size_in_bytes(full_leaves);
    }

private:
    void insert(std::string file_name, std::string file_path)
    {
        if (File_info* res = search(file_name, file_path); res != nullptr) // File already exist.
            return;

        m_files.push_back(std::make_unique<File_info>(std::move(file_name)));
        File_info* file = m_files.back().get();

        auto res = m_file_paths.insert(file_path, std::vector{file});
        if (!res)
            res->value().push_back(file);

        file->set_path(res->key_to_string_view());

        // TODO: Create emplace to avoid this nonsence.
        auto ff_res = m_file_finder.insert_suffix(file->name(), std::vector{file});
        if (ff_res)
            return;

        auto& file_infos = *ff_res->value();
        assert(std::ranges::find(file_infos, file) == file_infos.end());
        file_infos.emplace_back(file);
    }

    void erase(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (res == nullptr)
            return;

        auto& files_on_path = res->value();

        File_info* file;

        auto fpaths_it = std::ranges::find_if(
            files_on_path, [&](const File_info* fop) { return fop->name() == file_name; });

        if (fpaths_it == files_on_path.end())
            return;

        file = *fpaths_it;

        files_on_path.erase(fpaths_it);
        if (files_on_path.empty())
            m_file_paths.erase(file_path);

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

        auto vit = std::ranges::find_if(
            m_files, [&](const auto& unique_file) { return unique_file.get() == file; });

        assert(vit != m_files.end());
        m_files.erase(vit);
    }

    File_info* search(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (!res)
            return nullptr;

        const auto& files = res->value();
        for (File_info* file : files)
            if (file->name() == file_name)
                return file;

        return nullptr;
    }

private:
    // Vector of file pointers for quick reach of all files.
    //
    std::vector<std::unique_ptr<File_info>> m_files;

    // Trie that will hold files where key is full file path.
    ///
    art::ART<std::vector<File_info*>> m_file_paths;

    // Trie that holds all suffixes of all file names, which enables file search by file name.
    // File name is not unique, so we must hold vector of file pointers.
    //
    suffix_trie::Suffix_trie<std::vector<File_info*>> m_file_finder;
};

// NOLINTEND

#endif // FILES_H
