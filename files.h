#ifndef FILES_H
#define FILES_H

#include <bitset>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "art.h"
#include "os.h"
#include "small_string.h"
#include "types.h"
#include "util.h"

// NOLINTBEGIN(readability-implicit-bool-conversion, readability-redundant-access-specifiers,
// hicpp-explicit-conversions)

namespace fs = std::filesystem;

class FileInfo {
public:
    FileInfo() = default;

    explicit FileInfo(const std::string& file_name) : m_name{file_name} {}

    FileInfo(const std::string& file_name, std::string_view file_path)
        : m_name{file_name}
        , m_path{file_path}
    {
        if (!file_path.ends_with(file_name))
            throw std::runtime_error{"File path does not end with file name."};
    }

    [[nodiscard]] constexpr const SmallString& name() const noexcept { return m_name; }

    [[nodiscard]] const std::string_view& path() const noexcept { return m_path; }

    [[nodiscard]] std::string full_path() const noexcept
    {
        return std::format("{}{}", path(), name().c_str());
    }

    void set_path(std::string_view path) { m_path = path; }

private:
    SmallString m_name;      // File name with extension.
    std::string_view m_path; // Full file path.
};

static fs::path parent_path(const fs::path& path)
{
    fs::path parrent = path.parent_path();
    if (parrent == path.root_path())
        return parrent;

    parrent += os::path_sep_str;
    return parrent;
}

/**
 * Class that holds all file system files, their paths, size infos, etc.
 */
class Files {
public:
    static constexpr sz usize_max = std::numeric_limits<sz>::max();
    static constexpr sz objects_max = 80;
    static constexpr sz match_max = 256;

    /**
     * Struct that holds file info pointer and offset at which we matched file name. Offset is
     * used to highlight matched string with different color for easy visualization on console.
     */
    struct Match {
        const FileInfo* m_file;
        std::bitset<match_max> m_match_bs;
    };

    /**
     * Struct that holds vector of matches and total number of objects matched. Since number of
     * matches can be limited (no need to put all objects in a results if user limits it), we need
     * to separate results from number of objects matched.
     */
    class Matches {
    public:
        Matches(sz limit = objects_max) : m_limit(limit) { m_results.reserve(m_limit); }

        /**
         * Inserts other matches into the final matches.
         */
        void insert(Matches& other)
        {
            if (m_results.size() < m_limit) {
                const std::vector<Match>& other_res = other.m_results;
                sz ins = std::min(m_limit - m_results.size(), other_res.size());

                if (ins > 0)
                    m_results.insert(m_results.end(), other_res.begin(), other_res.begin() + ins);
            }

            m_objects += other.m_objects;
        }

        /**
         * Inserts other matches into the final matches.
         */
        void insert(const Matches& other)
        {
            if (m_results.size() < m_limit) {
                const std::vector<Match>& other_res = other.m_results;
                sz ins = std::min(m_limit - m_results.size(), other_res.size());

                if (ins > 0)
                    m_results.insert(m_results.end(), other_res.begin(), other_res.begin() + ins);
            }

            m_objects += other.m_objects;
        }

        template<class... Args>
        void insert(Args&&... args)
        {
            if (m_objects < m_limit)
                m_results.emplace_back(std::forward<Args>(args)...);

            ++m_objects;
        }

        void clear() noexcept
        {
            m_results.clear();
            m_objects = 0;
        }

        const std::vector<Match>& data() const noexcept { return m_results; }

        sz objects_count() const noexcept { return m_objects; }

        sz size() const noexcept { return m_results.size(); }

        bool empty() const noexcept { return m_objects == 0; }

        bool full() const noexcept { return m_results.size() == m_limit; }

        const Match& operator[](sz idx) const noexcept
        {
            assert(idx < m_results.size());
            return m_results[idx];
        }

    private:
        std::vector<Match> m_results;
        sz m_objects = 0;
        sz m_limit;
    };

    /**
     * Class that wraps insert result.
     * It holds a pointer to Leaf and a bool flag representing whether insert succeeded (read insert
     * for more details).
     */
    class result {
    public:
        result(FileInfo* value, bool ok) : m_value{value}, m_ok{ok} { assert(m_value != nullptr); }

        FileInfo* get() noexcept { return m_value; }

        [[nodiscard]] const FileInfo* get() const noexcept { return m_value; }

        [[nodiscard]] constexpr bool ok() const noexcept { return m_ok; }

        FileInfo* operator->() noexcept { return get(); }

        const FileInfo* operator->() const noexcept { return get(); }

        constexpr operator bool() const noexcept { return ok(); }

    private:
        FileInfo* m_value;
        bool m_ok;
    };

    /**
     * Inserts file into the files. Path is slit to filename and file path (with path separator "/"
     * or "\\").
     */
    result insert(const fs::path& path)
    {
        return insert(path.filename().string(), parent_path(path).string());
    }

    /**
     * Erases file from files.
     */
    void erase(const fs::path& path)
    {
        erase(path.filename().string(), parent_path(path).string());
    }

    auto search(const std::string& regex)
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
    Matches partial_search(const std::string& regex, sz slice_count, sz slice_number) const noexcept
    {
        assert(slice_count > slice_number);

        Matches matches;
        sz slash_pos = regex.find_last_of(os::path_sep);

        std::string search_name{slash_pos != std::string::npos ? regex.substr(slash_pos + 1) :
                                                                 regex};
        std::string search_path{slash_pos != std::string::npos ? regex.substr(0, slash_pos) : ""};

        if (!search_path.empty() && !m_file_paths.search_prefix_node(search_path))
            return matches;

        sz chunk = std::max(sz(1), m_files.size() / slice_count);
        auto file = m_files.begin() + chunk * slice_number;
        const auto& last = slice_count == slice_number + 1 ? m_files.end() : file + chunk;

        std::vector<std::string> parts{string_split(search_name, "*")};

        for (; file < last; ++file) {
            const SmallString& file_name = (*file)->name();
            const std::string_view& file_path = (*file)->path();

            const bool on_path = search_path.empty() || file_path.starts_with(search_path);
            if (!on_path)
                continue;

            if (!match_name(file_name, parts))
                continue;

            if (matches.full()) {
                matches.insert();
                continue;
            }

            match_slow(matches, parts, file_name, file_path, search_path, (*file).get());
        }

        return matches;
    }

    /**
     * File name match.
     * It iterates over all parts (strings in the original string separated by *) and checks if file
     * name constains them in order.
     */
    bool match_name(const SmallString& file_name,
                    const std::vector<std::string>& parts) const noexcept

    {
        sz offset = 0;
        for (const std::string& part : parts) {
            if (part.empty())
                continue;

            offset = file_name.find(part, offset);
            if (offset == SmallString::npos)
                return false;

            offset += part.size();
        }

        return true;
    }

    /**
     * Slow file name match.
     * Similar to fast match, it iterates over all parts (strings in the original string separated
     * by *) and checks if file name constains them in order. If all checks passed, it inserts a
     * full match. Slow means additional tracking of a matched characters positions. We will keep
     * matched letters in a bitset which will later be used to highlight matched text.
     */
    void match_slow(Matches& matches, const std::vector<std::string>& parts,
                    const SmallString& file_name, const std::string_view& file_path,
                    const std::string& search_path, const FileInfo* file_info) const noexcept

    {
        assert(!matches.full());

        std::bitset<match_max> match_bs;
        sz offset = 0;

        for (const std::string& part : parts) {
            if (part.empty())
                continue;

            offset = file_name.find(part, offset);
            if (offset == SmallString::npos)
                return;

            std::bitset<match_max> match_count{(sz(1) << part.size()) - 1};
            sz shift = file_path.size() + offset;
            match_bs |= match_count << shift;

            offset += part.size();
        }

        for (sz i = 0; i < search_path.size(); ++i)
            match_bs.set(i);

        matches.insert(file_info, match_bs);
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

        /* This must be done after removing file from m_files, since file's path is in file
         * paths, and we won't be able to match path when searching for file.
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

// NOLINTEND(readability-implicit-bool-conversion, readability-redundant-access-specifiers,
// hicpp-explicit-conversions)

#endif // FILES_H
