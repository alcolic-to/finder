#ifndef ARRAY_MAP_H
#define ARRAY_MAP_H

#include <algorithm>
#include <format>
#include <stdexcept>
#include <vector>

#include "types.h"

template<class T>
class ArrayMap {
public:
    static constexpr sz npos = -1;

    void insert(sz key, T value) { emplace(key, std::move(value)); }

    template<class... Args>
    void emplace(sz key, Args&&... args)
    {
        if (key >= m_idxs.size()) {
            const sz resize = std::max({m_idxs.size() * 2, key, sz(1)});
            m_idxs.resize(resize, npos);
        }

        if (m_idxs[key] != npos) {
            m_data[m_idxs[key]] = T(std::forward<Args>(args)...);
            return;
        }

        sz idx = m_data.size();
        m_data.emplace_back(std::forward<Args>(args)...);

        m_idxs[key] = idx;
        ++m_size;
    }

    void erase(sz key)
    {
        (*this)[key].~T();
        m_idxs[key] = npos;

        --m_size;
    }

    T& operator[](const sz key)
    {
        if (!contains(key))
            throw std::runtime_error{std::format("Invalid key {}", key)};

        return m_data[m_idxs[key]];
    }

    const T& operator[](const sz key) const
    {
        if (!contains(key))
            throw std::runtime_error{std::format("Invalid key {}", key)};

        return m_data[m_idxs[key]];
    }

    [[nodiscard]] bool contains(const sz key) const noexcept
    {
        return key < m_idxs.size() && m_idxs[key] != npos;
    }

    [[nodiscard]] bool empty() const noexcept { return m_size == 0; }

    /**
     * Iterators.
     */
    auto begin() noexcept { return m_data.begin(); }

    auto end() noexcept { return m_data.end(); }

    auto begin() const noexcept { return m_data.begin(); }

    auto end() const noexcept { return m_data.end(); }

private:
    std::vector<T> m_data;
    std::vector<sz> m_idxs;

    sz m_size = 0;
};

#endif // ARRAY_MAP
