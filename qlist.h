#ifndef QLIST_H
#define QLIST_H

#include <iostream>

// Debugging part
// #include <stdexcept>
#include <deque>
#include <stdexcept>

//

// TODO: Create move add (push) semantic.

template<class T, unsigned long long size>
class qlist {
    struct qlist_entry {
        T m_val;
        qlist_entry* m_prev;
        qlist_entry* m_next;
    };

public:
    constexpr qlist()
    {
        for (int i = 0; i < size; ++i) {
            m_list[i].m_prev = i > 0 ? &m_list[i - 1] : nullptr;
            m_list[i].m_next = i < size - 1 ? &m_list[i + 1] : nullptr;
        }

        m_free_head = &m_list[0];

        m_occu_head = nullptr;
        m_occu_tail = nullptr;
    }

    constexpr void push_front(const T& v)
    {
        // if (m_size == capacity())
        //     throw std::out_of_range{"List is full."};

        qlist_entry* entry = m_free_head;
        // assert(entry != nullptr);

        m_free_head = m_free_head->m_next;

        if (m_free_head != nullptr)
            m_free_head->m_prev = nullptr;

        entry->m_next = nullptr;
        entry->m_prev = m_occu_head;

        if (m_occu_head == nullptr)
            m_occu_head = m_occu_tail = entry;
        else {
            m_occu_head->m_next = entry;
            m_occu_head = entry;
        }

        entry->m_val = v;

        ++m_size;
    }

    constexpr void push_back(const T& v)
    {
        // if (m_size == capacity())
        //     throw std::out_of_range{"List is full."};

        qlist_entry* entry = m_free_head;
        // assert(entry != nullptr);

        m_free_head = m_free_head->m_next;

        if (m_free_head != nullptr)
            m_free_head->m_prev = nullptr;

        entry->m_next = m_occu_tail;
        entry->m_prev = nullptr;

        if (m_occu_tail == nullptr)
            m_occu_tail = m_occu_head = entry;
        else {
            m_occu_tail->m_prev = entry;
            m_occu_tail = entry;
        }

        entry->m_val = v;

        ++m_size;
    }

    constexpr void remove(qlist_entry* entry)
    {
        // if (m_size == 0)
        //     throw std::out_of_range{"List is empty."};

        if (m_occu_head == m_occu_tail && entry == m_occu_head)
            m_occu_head = m_occu_tail = nullptr;
        else if (entry == m_occu_head) {
            m_occu_head = m_occu_head->m_prev;
            m_occu_head->m_next = nullptr;
        }
        else if (entry == m_occu_tail) {
            m_occu_tail = m_occu_tail->m_next;
            m_occu_tail->m_prev = nullptr;
        }
        else {
            entry->m_prev->m_next = entry->m_next;
            entry->m_next->m_prev = entry->m_prev;
        }

        entry->m_prev = nullptr;
        entry->m_next = m_free_head;

        if (m_free_head != nullptr)
            m_free_head->m_prev = entry;

        m_free_head = entry;

        entry->m_val.~T();
        --m_size;
    }

    constexpr void remove(const T& v)
    {
        for (qlist_entry* entry = m_occu_tail; entry != nullptr; entry = entry->m_next) {
            if (entry->m_val == v) {
                remove(entry);
                return;
            }
        }
    }

    constexpr void pop_back() { remove(m_occu_tail); }

    constexpr void pop_front() { remove(m_occu_head); }

    constexpr bool empty() { return m_size == 0; }

    constexpr T& front() { return m_occu_head->m_val; }

    constexpr T& back() { return m_occu_tail->m_val; }

    constexpr unsigned long long capacity() { return size; }

    // Debugging tools.
    //
    void print()
    {
        // std::cout << "Head: " << m_head->m_val << " Tail: " << m_tail->m_val << "\n";

        // for (qlist_entry* curr = m_head; curr != nullptr; curr = curr->m_next)
        //     std::cout << (curr != m_head ? " <--> " : "") << "(" << curr->m_prev << "|" <<
        //     curr->m_val << "|" << curr->m_next << ")";

        // std::cout << "\n";

        if (m_occu_head == nullptr) {
            std::cout << "Empty list.\n";
            return;
        }

        for (qlist_entry* curr = m_occu_head; curr != nullptr; curr = curr->m_prev)
            std::cout << ((curr == m_occu_head) ? "" : " ") << curr->m_val;

        std::cout << "\n";
    }

    bool identical(std::deque<T>& q)
    {
        qlist_entry* lit = m_occu_head;
        auto qit = q.begin();

        for (; lit != nullptr && qit != q.end(); lit = lit->m_prev, ++qit)
            if (lit->m_val != *qit)
                throw std::logic_error{"Not identical!"};

        if (lit != nullptr || qit != q.end())
            throw std::logic_error{"Not identical!"};

        qlist_entry* lrit = m_occu_tail;
        auto qrit = q.rbegin();

        for (; lrit != nullptr && qrit != q.rend(); lrit = lrit->m_next, ++qrit)
            if (lrit->m_val != *qrit)
                throw std::logic_error{"Not identical!"};

        if (lrit != nullptr || qrit != q.rend())
            throw std::logic_error{"Not identical!"};

        return true;
    }

private:
    qlist_entry* m_free_head = nullptr;
    qlist_entry* m_occu_head = nullptr;
    qlist_entry* m_occu_tail = nullptr;
    unsigned long long m_size = 0;
    qlist_entry m_list[size];
};

#endif