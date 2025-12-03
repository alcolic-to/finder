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

#ifndef FINDER_QUERY_HPP
#define FINDER_QUERY_HPP

struct Query {
    std::string m_pinned;
    std::string m_query;

    [[nodiscard]] std::string full() const { return m_pinned + m_query; }
};

#endif // FINDER_QUERY_HPP
