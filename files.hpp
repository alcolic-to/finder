/**
 * Copyright 2025, Aleksandar Colic
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#ifndef FINDER_FILES_HPP
#define FINDER_FILES_HPP

#include <bitset>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "array_map.hpp"
#include "art.hpp"
#include "os.hpp"
#include "small_string.hpp"
#include "types.hpp"
#include "util.hpp"

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

    [[nodiscard]] constexpr const stl::SmallString& name() const noexcept { return m_name; }

    [[nodiscard]] const std::string_view& path() const noexcept { return m_path; }

    [[nodiscard]] std::string full_path() const noexcept
    {
        return std::format("{}{}", path(), name().c_str());
    }

    void set_path(std::string_view path) { m_path = path; }

private:
    stl::SmallString m_name; // File name with extension.
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
    static constexpr usize usize_max = std::numeric_limits<usize>::max();
    static constexpr usize objects_max = 80;
    static constexpr usize match_max = 256;

    /**
     * Match rank.
     */
    class Rank {
    public:
        constexpr Rank(usize rank = 3) : m_rank{rank} {}

        constexpr operator bool() const noexcept { return m_rank < 3; }

        constexpr operator usize() const noexcept { return m_rank; }

        constexpr bool r1() const noexcept { return m_rank == 0; }

        constexpr bool r2() const noexcept { return m_rank == 1; }

        constexpr bool r3() const noexcept { return m_rank == 2; }

    private:
        usize m_rank;
    };

    inline static const Rank RankLast{3};

    /**
     * Struct that holds file info pointer and offset at which we matched file name. Offset is
     * used to highlight matched string with different color for easy visualization on console.
     */
    struct Match {
        const FileInfo* m_file;
        std::bitset<match_max> m_match_bs;

        const FileInfo* operator->() const { return m_file; }
    };

    /**
     * Struct that holds vector of matches and total number of objects matched. Since number of
     * matches can be limited (no need to put all objects in a results if user limits it), we need
     * to separate results from number of objects matched.
     */
    class Matches {
    public:
        Matches(usize limit = objects_max) : m_limit(limit) { m_results.reserve(m_limit); }

        const std::vector<Match>& data() const noexcept { return m_results; }

        usize objects_count() const noexcept { return m_objects; }

        usize size() const noexcept { return m_results.size(); }

        bool empty() const noexcept { return m_objects == 0; }

        bool full() const noexcept { return m_results.size() == m_limit; }

        usize r1_start() const noexcept { return 0; }

        usize r2_start() const noexcept { return m_r1; }

        usize r3_start() const noexcept { return m_r1 + m_r2; }

        usize r1_end() const noexcept { return r2_start(); }

        usize r2_end() const noexcept { return r3_start(); }

        usize r3_end() const noexcept { return m_results.size(); }

        auto r1_start_it() const noexcept { return m_results.begin() + r1_start(); }

        auto r2_start_it() const noexcept { return m_results.begin() + r2_start(); }

        auto r3_start_it() const noexcept { return m_results.begin() + r3_start(); }

        auto r1_end_it() const noexcept { return m_results.begin() + r1_end(); }

        auto r2_end_it() const noexcept { return m_results.begin() + r1_end(); }

        auto r3_end_it() const noexcept { return m_results.begin() + r1_end(); }

        bool can_insert(Rank r) const noexcept
        {
            if (r.r1())
                return r1_end() < m_limit;

            if (r.r2())
                return r2_end() < m_limit;

            return r3_end() < m_limit;
        }

        const Match& operator[](usize idx) const noexcept
        {
            assert(idx < m_results.size());
            return m_results[idx];
        }

        /**
         * Inserts other matches into the final matches.
         */
        void insert(const Matches& other)
        {
            // if (m_results.size() < m_limit) {
            //     const std::vector<Match>& other_res = other.m_results;
            //     usize ins = std::min(m_limit - m_results.size(), other_res.size());

            //     if (ins > 0)
            //         m_results.insert(m_results.end(), other_res.begin(), other_res.begin() +
            //         ins);
            // }

            // m_objects += other.m_objects;

            const std::vector<Match>& other_res = other.m_results;
            if (m_r1 < m_limit) {
                usize c = std::min(other.m_r1, m_limit - m_r1);
                if (c > 0) {
                    m_results.insert(m_results.begin() + m_r1, other_res.begin(),
                                     other_res.begin() + c);

                    m_r1 += c;
                }

                if (m_results.size() > m_limit)
                    m_results.resize(m_limit);
            }

            if (m_r1 + m_r2 < m_limit) {
                usize c = std::min(other.m_r2, m_limit - m_r1 - m_r2);
                if (c > 0) {
                    m_results.insert(m_results.begin() + m_r1, other_res.begin() + other.m_r1,
                                     other_res.begin() + other.m_r1 + c);

                    m_r2 += c;
                }

                if (m_results.size() > m_limit)
                    m_results.resize(m_limit);
            }

            if (m_results.size() < m_limit) {
                usize c = std::min(m_limit - m_results.size(), other_res.size());
                if (c > 0)
                    m_results.insert(m_results.end(), other_res.begin() + other.m_r1 + other.m_r2,
                                     other_res.begin() + other.m_r1 + other.m_r2 + c);
            }

            m_objects += other.m_objects;
        }

        void inc_objects() noexcept { ++m_objects; }

        template<class... Args>
        void insert(Rank rank, Args&&... args)
        {
            if (full()) {
                assert(rank.first() || rank.second());
                m_results.pop_back();

                if (r1_end() >= m_limit)
                    --m_r1;
                else if (r2_end() >= m_limit)
                    --m_r2;
            }

            auto it = r1_end_it();

            if (rank.r1()) {
                it = r1_end_it();
                ++m_r1;
            }
            else if (rank.r2()) {
                it = r2_end_it();
                ++m_r2;
            }
            else {
                assert(rank.third());
                it = r3_end_it();
            }

            m_results.emplace(it, std::forward<Args>(args)...);
            inc_objects();
        }

        void clear() noexcept
        {
            m_results.clear();
            m_r1 = 0;
            m_r2 = 0;
            m_objects = 0;
        }

    private:
        std::vector<Match> m_results;
        usize m_objects = 0;
        usize m_r1 = 0;
        usize m_r2 = 0;
        usize m_limit;
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

    /**
     * Searches for files with provided regex.
     */
    Matches search(const std::string& regex) const noexcept { return partial_search(regex, 1, 0); }

    /**
     * Partial files search user for multithreaded search. User should provide number of slices
     * (threads) and a slice number (thread number) that is used for search.
     * Slice number is 0 based.
     */
    Matches partial_search(const std::string& regex, usize slice_count,
                           usize slice_number) const noexcept
    {
        TZoneScopedC(tracy::Color::Green1);

        assert(slice_count > slice_number);

        Matches matches;
        usize slash_pos = regex.find_last_of(os::path_sep);

        std::string search_name{slash_pos != std::string::npos ? regex.substr(slash_pos + 1) :
                                                                 regex};
        std::string search_path{slash_pos != std::string::npos ? regex.substr(0, slash_pos) : ""};

        if (!search_path.empty() && !m_file_paths.search_prefix_node(search_path))
            return matches;

        usize chunk = std::max(usize(1), m_files.size() / slice_count);
        auto file = m_files.begin() + chunk * slice_number;
        if (file >= m_files.end())
            return matches;

        const auto& end = slice_count == slice_number + 1 ? m_files.end() : file + chunk;

        std::vector<std::string> parts{string_split(search_name, "*")};

        for (; file < end; ++file) {
            const stl::SmallString& file_name = file->name();
            const std::string_view& file_path = file->path();

            const bool on_path = search_path.empty() || file_path.starts_with(search_path);
            if (!on_path)
                continue;

            Rank r = match_name(file_name, parts);
            if (!r)
                continue;

            if (!matches.can_insert(r)) {
                matches.inc_objects();
                continue;
            }

            match_slow(matches, r, parts, file_name, file_path, search_path, &*file);
        }

        return matches;
    }

    /**
     * File name match.
     * It iterates over all parts (strings in the original string separated by *) and checks if file
     * name constains them in order.
     */
    [[clang::always_inline]] Rank match_name(const stl::SmallString& file_name,
                                             const std::vector<std::string>& parts) const noexcept
    {
        Rank rank{2}; // Default rank.
        usize offset = 0;
        for (const std::string& part : parts) {
            if (part.empty())
                continue;

            offset = file_name.find(part, offset);
            if (offset == stl::SmallString::npos)
                return RankLast;

            if (offset == 0)
                // TODO: Check whether this is performant, because we are calling strlen for
                // file_name.size()
                rank = part.size() == file_name.size() ? 0 : 1;

            offset += part.size();
        }

        return rank;
    }

    /**
     * Slow file name match.
     * Similar to fast match, it iterates over all parts (strings in the original string separated
     * by *) and checks if file name constains them in order. If all checks passed, it inserts a
     * full match. Slow means additional tracking of a matched characters positions. We will keep
     * matched letters in a bitset which will later be used to highlight matched text.
     */
    void match_slow(Matches& matches, Rank rank, const std::vector<std::string>& parts,
                    const stl::SmallString& file_name, const std::string_view& file_path,
                    const std::string& search_path, const FileInfo* file_info) const noexcept
    {
        std::bitset<match_max> match_bs;
        usize offset = 0;
        for (const std::string& part : parts) {
            if (part.empty())
                continue;

            offset = file_name.find(part, offset);
            assert(offset != stl::SmallString::npos);

            std::bitset<match_max> match_count{(usize(1) << part.size()) - 1};
            usize shift = file_path.size() + offset;
            match_bs |= match_count << shift;

            offset += part.size();
        }

        for (usize i = 0; i < search_path.size(); ++i)
            match_bs.set(i);

        matches.insert(rank, file_info, match_bs);
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
        if (FileInfo* res = find(file_name, file_path); res != nullptr) // File already exist.
            return {res, false};

        static usize guid{0};
        usize file_guid = guid++;

        m_files.emplace(file_guid, file_name);
        FileInfo& file = m_files[file_guid];
        assert(file.name() == file_name);

        m_file_paths[file_path].push_back(file_guid);
        file.set_path(m_file_paths.leaf_from_value(m_file_paths[file_path])->key_to_string_view());
        assert(file.path() == file_path);

        return {&file, true};
    }

    void erase(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (res == nullptr)
            return;

        std::vector<usize>& files_on_path = res->value();
        auto fpaths_it = std::ranges::find_if(
            files_on_path, [&](usize guid) { return m_files[guid].name() == file_name; });

        if (fpaths_it == files_on_path.end())
            return;

        files_on_path.erase(fpaths_it);

        auto file_it = std::ranges::find_if(m_files, [&](const FileInfo& file) {
            return file.name() == file_name && file.path() == file_path;
        });

        assert(file_it != m_files.end());
        m_files.erase(file_it);

        /**
         * This must be done after removing file from m_files, since file's path is in file
         * paths, and we won't be able to match path when searching for file.
         */
        if (files_on_path.empty())
            m_file_paths.erase(file_path);
    }

    /**
     * Finds single file with provided name and file path if it exists.
     */
    FileInfo* find(const std::string& file_name, const std::string& file_path)
    {
        auto res = m_file_paths.search(file_path);
        if (!res)
            return nullptr;

        const auto& files = res->value();
        for (usize guid : files) {
            FileInfo& file = m_files[guid];
            if (file.name() == file_name)
                return &file;
        }

        return nullptr;
    }

private:
    // Container with file infos.
    stl::ArrayMap<FileInfo> m_files;

    // Trie that holds file info indexes, where key is the full file path.
    stl::ART<std::vector<usize>> m_file_paths;
};

// NOLINTEND(readability-implicit-bool-conversion, readability-redundant-access-specifiers,
// hicpp-explicit-conversions)

#endif // FINDER_FILES_HPP
