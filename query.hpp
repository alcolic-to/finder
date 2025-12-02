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
