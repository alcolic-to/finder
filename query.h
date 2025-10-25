#include <string>

#ifndef QUERY_H
#define QUERY_H

struct Query {
    std::string m_pinned;
    std::string m_query;

    [[nodiscard]] std::string full() const { return m_pinned + m_query; }
};

#endif // QUERY_H
