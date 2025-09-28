#ifndef FILES_H
#define FILES_H

#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "art.h"
#include "os.h"
#include "small_string.h"
#include "util.h"

// NOLINTBEGIN

namespace fs = std::filesystem;

class FileInfo {
public:
    FileInfo() = default;

    FileInfo(const std::string& file_name) : m_name{file_name} {}

    FileInfo(std::string file_name, std::string_view file_path)
        : m_name{std::move(file_name)}
        , m_path{std::move(file_path)}
    {
        if (!file_path.ends_with(file_name))
            throw std::runtime_error{"File path does not end with file name."};
    }

    [[nodiscard]] constexpr const SmallString& name() const noexcept { return m_name; }

    [[nodiscard]] const std::string_view path() const noexcept { return m_path; }

    [[nodiscard]] std::string full_path() const noexcept
    {
        if (path() == "/" || path() == "C:\\")
            return std::format("{}{}", path(), name().c_str());

        return std::format("{}{}{}", path(), os::path_sep, name().c_str());
    }

    void set_path(std::string_view path) { m_path = path; }

private:
    SmallString m_name;      // File name with extension.
    std::string_view m_path; // Full file path.
};

// Class that holds all file system files, their paths, size infos, etc.
//
class Files {
public:
    // Class that wraps insert result.
    // It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
    // for more details).
    //
    class result {
    public:
        result(FileInfo* value, bool ok) : m_value{value}, m_ok{ok} { assert(m_value != nullptr); }

        FileInfo* get() noexcept { return m_value; }

        const FileInfo* get() const noexcept { return m_value; }

        constexpr bool ok() const noexcept { return m_ok; }

        FileInfo* operator->() noexcept { return get(); }

        const FileInfo* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        FileInfo* m_value;
        bool m_ok;
    };

    static constexpr size_t search_limit = 128;

    result insert(const fs::path& path)
    {
        return insert(path.filename().string(), path.parent_path().string());
    }

    void erase(const fs::path& path)
    {
        erase(path.filename().string(), path.parent_path().string());
    }

    auto search(const std::string& regex, size_t limit = std::numeric_limits<size_t>::max())
    {
        std::vector<const FileInfo*> results;
        results.reserve(1024 * 1024);

        sz slash_pos = regex.find_last_of(os::path_sep);
        std::string name{slash_pos != std::string::npos ? regex.substr(slash_pos + 1) : regex};
        std::string path{slash_pos != std::string::npos ? regex.substr(0, slash_pos) : ""};

        if (!m_file_paths.search_prefix_node(path))
            return results;

        for (const auto& file : m_files)
            if (file->path().starts_with(path) && std::strstr(file->name(), name.c_str()))
                results.emplace_back(file.get());

        return results;
    }

    /**
     * Partial files search user for multithreaded search. User should provide number of slices
     * (threads) and a slice number (thread number) that is used for search.
     * Slice number is 0 based.
     */
    auto partial_search(const std::string& regex, sz slice_count, sz slice_number) const noexcept
    {
        assert(slice_count > slice_number);

        std::vector<const FileInfo*> results;
        results.reserve(1024);

        sz slash_pos = regex.find_last_of(os::path_sep);
        std::string name{slash_pos != std::string::npos ? regex.substr(slash_pos + 1) : regex};
        std::string path{slash_pos != std::string::npos ? regex.substr(0, slash_pos) : ""};

        if (!m_file_paths.search_prefix_node(path))
            return results;

        sz chunk = std::max(sz(1), m_files.size() / slice_count);
        auto file = m_files.begin() + chunk * slice_number;
        const auto& last = slice_count == slice_number + 1 ? m_files.end() : file + chunk;

        for (; file < last; ++file)
            if ((*file)->path().starts_with(path) && (*file)->name().contains(name))
                results.emplace_back((*file).get());

        return results;
    }

    auto files_count() { return m_files.size(); }

    auto files_size()
    {
        return m_files.size() * (sizeof(FileInfo) + sizeof(std::unique_ptr<FileInfo>));
    }

    auto file_paths_leaves_count() { return m_file_paths.leaves_count(); }

    auto file_paths_size(bool full_leaves = true)
    {
        return m_file_paths.size_in_bytes(full_leaves);
    }

    void print_stats()
    {
        std::cout << "-------------------------------\n";
        std::cout << "Files count: " << m_files.size() << "\n";
        std::cout << "-------------------------------\n";

        std::cout << "File paths stats:\n";
        m_file_paths.print_stats();
    }

private:
    result insert(std::string file_name, std::string file_path)
    {
        if (FileInfo* res = search(file_name, file_path); res != nullptr) // File already exist.
            return {res, false};

        m_files.push_back(std::make_unique<FileInfo>(file_name));
        FileInfo* file = m_files.back().get();

        m_file_paths[file_path].push_back(file);
        file->set_path(m_file_paths.leaf_from_value(m_file_paths[file_path])->key_to_string_view());
        return {file, true};
    }

    void erase(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (res == nullptr)
            return;

        std::vector<FileInfo*>& files_on_path = res->value();
        auto fpaths_it = std::ranges::find_if(
            files_on_path, [&](const FileInfo* fop) { return fop->name() == file_name; });

        if (fpaths_it == files_on_path.end())
            return;

        files_on_path.erase(fpaths_it);

        auto file_it = std::ranges::find_if(m_files, [&](const std::unique_ptr<FileInfo>& file) {
            return file->name() == file_name && file->path() == file_path;
        });

        assert(file_it != m_files.end());
        m_files.erase(file_it);

        /* This must be done after removing file from m_files, since file's path is in file paths,
         * and we won't be able to match path when searching for file.
         */
        if (files_on_path.empty())
            m_file_paths.erase(file_path);
    }

    FileInfo* search(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (!res)
            return nullptr;

        const auto& files = res->value();
        for (FileInfo* file : files)
            if (file->name() == file_name)
                return file;

        return nullptr;
    }

private:
    // Vector of files.
    //
    std::vector<std::unique_ptr<FileInfo>> m_files;

    // Trie that holds file info pointers, where key is the full file path.
    ///
    art::ART<std::vector<FileInfo*>> m_file_paths;
};

// NOLINTEND

#endif // FILES_H
