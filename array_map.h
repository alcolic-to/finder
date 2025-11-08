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
            const sz resize = std::max({m_idxs.size() * 2, key + 1, sz(1)});
            m_idxs.resize(resize, npos);
        }

        if (m_idxs[key] != npos) {
            m_data[m_idxs[key]] = T(std::forward<Args>(args)...);
            return;
        }

        m_idxs[key] = m_data.size();
        m_data.emplace_back(std::forward<Args>(args)...);
        m_back_idxs.emplace_back(key);

        ++m_size;
    }

    /**
     * Removes item by taking value from last element and poping last.
     */
    void erase(sz key)
    {
        if (!contains(key))
            throw std::runtime_error{std::format("Invalid key {}", key)};

        sz rm_idx = m_idxs[key];
        T& rm_entry = m_data[rm_idx];

        if (rm_idx < m_data.size() - 1) {
            rm_entry = std::move(m_data[m_data.size() - 1]);
            m_back_idxs[rm_idx] = m_back_idxs[m_back_idxs.size() - 1];
            m_idxs[m_back_idxs[rm_idx]] = rm_idx;
        }

        m_data.pop_back();
        m_back_idxs.pop_back();
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

    [[nodiscard]] sz size() const noexcept { return m_size; }

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
    std::vector<sz> m_back_idxs;

    sz m_size = 0;
};

#endif // ARRAY_MAP
