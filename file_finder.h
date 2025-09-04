#ifndef FILE_FINDER_H
#define FILE_FINDER_H

#include <cstddef>
#include <filesystem>
#include <limits>
#include <memory>
#include <ranges>
#include <string>
#include <vector>

#include "art.h"
#include "ast.h"
#include "os.h"
#include "small_string.h"
#include "util.h"

// NOLINTBEGIN

namespace fs = std::filesystem;

class File_info {
public:
    File_info() = default;

    File_info(const std::string& file_name) : m_name{file_name} {}

    File_info(const std::string& file_name, std::string_view file_path)
        : m_name{file_name}
        , m_path{std::move(file_path)}
    {
        if (!file_path.ends_with(file_name))
            throw std::runtime_error{"File path does not end with file name."};
    }

    [[nodiscard]] const char* name() const noexcept { return m_name; }

    [[nodiscard]] const std::string_view path() const noexcept { return m_path; }

    [[nodiscard]] std::string full_path() const noexcept
    {
        if (path() == "/" || path() == "C:\\")
            return std::format("{}{}", path(), name());

        const char sep = std::filesystem::path::preferred_separator;
        return std::format("{}{}{}", path(), sep, name());
    }

    void set_path(std::string_view path) { m_path = path; }

private:
    Small_string m_name;     // File name with extension.
    std::string_view m_path; // Full file path.
};

// Class that holds all file system files, their paths, size infos, etc.
// TODO: Files can be found by regex search.
//
class FileFinder {
public:
    // Class that wraps insert result.
    // It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
    // for more details).
    //
    class result {
    public:
        result(File_info* value, bool ok) : m_value{value}, m_ok{ok} { assert(m_value != nullptr); }

        File_info* get() noexcept { return m_value; }

        const File_info* get() const noexcept { return m_value; }

        constexpr bool ok() const noexcept { return m_ok; }

        File_info* operator->() noexcept { return get(); }

        const File_info* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        File_info* m_value;
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
        std::vector<const File_info*> results;

        sz slash_pos = regex.find_last_of(os::path_sep);
        std::string file{slash_pos != std::string::npos ? regex.substr(slash_pos + 1) : regex};
        std::string path{slash_pos != std::string::npos ? regex.substr(0, slash_pos) : ""};

        if (!m_file_paths.search_prefix_node(path))
            return results;

        while (file.starts_with('*'))
            file = file.substr(1);

        while (file.ends_with('*'))
            file = file.substr(0, file.size() - 1);

        m_file_finder.search_prefix_while(file, [&](const std::vector<File_info*>& infos) {
            for (const auto& file_info : infos) {
                if (file_info->path().starts_with(path) &&
                    std::ranges::find(results, file_info) == results.end())
                    results.push_back(file_info);

                if (results.size() >= limit)
                    return false;
            }

            return true;
        });

        return results;
    }

    auto files_count() { return m_files.size(); }

    auto files_size()
    {
        return m_files.size() * (sizeof(File_info) + sizeof(std::unique_ptr<File_info>));
    }

    auto file_paths_leaves_count() { return m_file_paths.leaves_count(); }

    auto file_finder_leaves_count() { return m_file_finder.leaves_count(); }

    auto file_paths_size(bool full_leaves = true)
    {
        return m_file_paths.size_in_bytes(full_leaves);
    }

    auto file_finder_size() { return m_file_finder.size_in_bytes(); }

    void print_stats()
    {
        std::cout << "-------------------------------\n";
        std::cout << "Files count: " << m_files.size() << "\n";
        std::cout << "-------------------------------\n";

        std::cout << "File paths stats:\n";
        m_file_paths.print_stats();

        std::cout << "File finder stats:\n";
        m_file_finder.print_stats();
    }

private:
    result insert(std::string file_name, std::string file_path)
    {
        if (File_info* res = search(file_name, file_path); res != nullptr) // File already exist.
            return {res, false};

        m_files.push_back(std::make_unique<File_info>(file_name));
        File_info* file = m_files.back().get();

        auto res = m_file_paths.insert(file_path, std::vector{file});
        if (!res)
            res->value().push_back(file);

        file->set_path(res->key_to_string_view());

        // TODO: Create emplace to avoid this nonsence.
        auto ff_res = m_file_finder.insert(file->name(), std::vector{file});
        if (ff_res)
            return {file, true};

        auto& file_infos = ff_res->value();
        assert(std::ranges::find(file_infos, file) == file_infos.end());
        file_infos.emplace_back(file);

        return {file, true};
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

        auto& file_infos = ff_res->value();
        auto fi_it = std::ranges::find(file_infos, file);
        assert(fi_it != file_infos.end());

        file_infos.erase(fi_it);
        if (file_infos.empty())
            m_file_finder.erase(file->name());

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
    // Vector of files.
    //
    std::vector<std::unique_ptr<File_info>> m_files;

    // Trie that holds file info pointers, where key is the full file path.
    ///
    art::ART<std::vector<File_info*>> m_file_paths;

    // Trie that holds all suffixes of all file names, which enables file search by file name.
    // File name is not unique, so we must hold vector of file pointers.
    //
    ast::AST<std::vector<File_info*>> m_file_finder;
};

// NOLINTEND

#endif // FILE_FINDER_H
