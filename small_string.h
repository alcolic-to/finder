#ifndef SMALL_STRING_H
#define SMALL_STRING_H
#include <bit>
#include <string.h>
#include <string>

#include "ptr_tag.h"

// NOLINTBEGIN

// Small string.
// If string is smaller than 7 bytes, data will be stored in pointer directly, otherwise m_data will
// point to allocated string. We will set pointer tag (small or big) which indicates whether string
// is small or big.
//
class Small_string {
    static constexpr size_t small_limit = 6;
    static constexpr uintptr_t small_tag = 0;
    static constexpr uintptr_t big_tag = 1;

public:
    Small_string() : m_data{nullptr} {}

    Small_string(const std::string& s) : Small_string(s.c_str(), s.size()) {}

    Small_string(const char* s, size_t size = 0) : m_data{nullptr}
    {
        if (s == nullptr)
            return;

        if (size == 0)
            size = std::strlen(s);

        if (size <= small_limit) {
            std::memcpy(m_sso + 1, s, size); // Small tag is set to 0 implicitly on a first byte.
            return;
        }

        ctr_from_big(s, size);
    }

    Small_string(const Small_string& other)
    {
        if (other.small())
            m_data = other.m_data;
        else
            ctr_from_big(other.data(), other.size());
    }

    Small_string(Small_string&& other) noexcept
    {
        m_data = other.m_data;
        other.m_data = nullptr;
    }

    ~Small_string()
    {
        if (empty() || small())
            return;

        delete[] data();
    }

    Small_string& operator=(const Small_string& other)
    {
        if (other.small())
            m_data = other.m_data;
        else
            ctr_from_big(other.data(), other.size());

        return *this;
    }

    Small_string& operator=(Small_string&& other) noexcept
    {
        m_data = other.m_data;
        other.m_data = nullptr;
        return *this;
    }

    [[nodiscard]] bool operator==(const std::string& other) const noexcept
    {
        return !std::strcmp(data(), other.c_str());
    }

    [[nodiscard]] bool operator==(const Small_string& other) const noexcept
    {
        return !std::strcmp(data(), other.data());
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return m_data == nullptr; }

    [[nodiscard]] constexpr bool small() const noexcept { return tag(m_data) == small_tag; }

    [[nodiscard]] constexpr bool big() const noexcept { return tag(m_data) == big_tag; }

    [[nodiscard]] const char* data() const noexcept
    {
        return small() ? &m_sso[1] : static_cast<char*>(clear_tag(m_data));
    }

    [[nodiscard]] operator const char*() const noexcept { return data(); }

    [[nodiscard]] size_t size() const noexcept { return std::strlen(*this); }

private:
    void ctr_from_big(const char* big, size_t size)
    {
        assert(size > small_limit);

        char* str = new char[size + 1];
        std::memcpy(str, big, size);
        str[size] = 0;
        m_data = set_tag(str, big_tag);
    }

    union {
        void* m_data;
        char m_sso[8];
    };
};

static_assert(sizeof(Small_string) == 8);

// NOLINTEND

#endif
