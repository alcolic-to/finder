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

#include <string>

#include "files.hpp"
#include "os.hpp"

#ifndef FINDER_QUERY_HPP
#define FINDER_QUERY_HPP

/**
 * User provided query.
 * It contains of pinned and query part. Pinned part is used as a pinned path which filters only
 * files on that path. Query part is query regex which we will use for search on a pinned path.
 */
class Query {
public:
    /**
     * Moves pinned query path one level down.
     * If pinned path is on the "seconds" level (/usr/), we will jump directly to the empty path,
     * because there is no point in pinning root only (/).
     */
    bool level_down()
    {
        if (m_pinned.empty())
            return false;

        m_pinned.pop_back();
        while (!m_pinned.empty() && m_pinned.back() != os::path_sep)
            m_pinned.pop_back();

        if (m_pinned == os::path_sep_str)
            m_pinned.clear();

        return true;
    }

    /**
     * Moves pinned query path one level up.
     * If pinned path is empty, we will jump directly to the "second" level, skipping root, because
     * there is no point in pinning root only (/).
     * We will try to go level up "smartly", meaning that we will remove part of path from user
     * query that will be pinned.
     */
    bool level_up(const Files::Match& match)
    {
        usize slash_pos = m_query.find_last_of(os::path_sep);
        std::string query_name{slash_pos != std::string::npos ? m_query.substr(slash_pos + 1) :
                                                                m_query};
        std::string query_path{slash_pos != std::string::npos ? m_query.substr(0, slash_pos + 1) :
                                                                ""};

        const std::string full_path = match.m_file->full_path().substr();

        if (m_pinned == match.m_file->path())
            query_name.clear();

        for (auto it = full_path.begin() + m_pinned.size(); it != full_path.end(); ++it) { // NOLINT
            m_pinned.append(1, *it);
            if (*it == os::path_sep && m_pinned != os::path_sep_str)
                break;
        }

        if (!m_pinned.ends_with(os::path_sep))
            m_pinned.append(1, os::path_sep);

        if (query_path.starts_with(os::path_sep))
            query_path = query_path.substr(1);

        slash_pos = query_path.find_first_of(os::path_sep);
        if (slash_pos != std::string::npos)
            query_path = query_path.substr(slash_pos + 1);

        m_query = query_path + query_name;
        return true;
    }

    /**
     * Pins path from picker position and removes path from name.
     */
    void pin_path(const Files::Match& match)
    {
        m_pinned = match.m_file->full_path();
        if (!m_pinned.ends_with(os::path_sep))
            m_pinned.append(1, os::path_sep);

        m_query.clear();
    }

    [[nodiscard]] std::string& pinned() { return m_pinned; }

    [[nodiscard]] const std::string& pinned() const { return m_pinned; }

    [[nodiscard]] std::string& query() { return m_query; }

    [[nodiscard]] const std::string& query() const { return m_query; }

    [[nodiscard]] std::string full() const { return m_pinned + m_query; }

private:
    std::string m_pinned;
    std::string m_query;
};

#endif // FINDER_QUERY_HPP
