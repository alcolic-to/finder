#ifndef SMALL_STRING_H
#define SMALL_STRING_H

#include <string.h>
#include <string>

#include "ptr_tag.h"

// NOLINTBEGIN

// Small string.
// If string is smaller than 7 bytes, data will be stored in pointer directly, otherwise m_data will
// point to allocated string. We will set pointer tag (small or big) which indicates whether string
// is small or big.
//
class SmallString {
    static constexpr size_t small_limit = 6;
    static constexpr uintptr_t small_tag = 0;
    static constexpr uintptr_t big_tag = 1;

public:
    constexpr SmallString() : m_data{nullptr} {}

    constexpr SmallString(const std::string& s) : SmallString(s.c_str(), s.size()) {}

    constexpr SmallString(const char* s, size_t size = 0) : m_data{nullptr}
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

    constexpr SmallString(const SmallString& other)
    {
        if (other.small())
            m_data = other.m_data;
        else
            ctr_from_big(other.c_str(), other.size());
    }

    SmallString(SmallString&& other) noexcept
    {
        m_data = other.m_data;
        other.m_data = nullptr;
    }

    ~SmallString()
    {
        if (empty() || small())
            return;

        delete[] c_str();
    }

    SmallString& operator=(const SmallString& other)
    {
        if (other.small())
            m_data = other.m_data;
        else
            ctr_from_big(other.c_str(), other.size());

        return *this;
    }

    SmallString& operator=(SmallString&& other) noexcept
    {
        m_data = other.m_data;
        other.m_data = nullptr;
        return *this;
    }

    [[nodiscard]] bool operator==(const char* other) const noexcept
    {
        return !std::strcmp(c_str(), other);
    }

    [[nodiscard]] bool operator==(const std::string& other) const noexcept
    {
        return !std::strcmp(c_str(), other.c_str());
    }

    [[nodiscard]] bool operator==(const SmallString& other) const noexcept
    {
        return !std::strcmp(c_str(), other.c_str());
    }

    [[nodiscard]] constexpr bool empty() const noexcept { return m_data == nullptr; }

    [[nodiscard]] constexpr const char* c_str() const noexcept
    {
        return small() ? &m_sso[1] : static_cast<char*>(clear_tag(m_data));
    }

    [[nodiscard]] constexpr std::string str() const noexcept { return std::string{c_str()}; }

    [[nodiscard]] constexpr operator const char*() const noexcept { return c_str(); }

    [[nodiscard]] constexpr operator bool() const noexcept { return !empty(); }

    [[nodiscard]] size_t size() const noexcept { return std::strlen(*this); }

    [[nodiscard]] bool starts_with(const SmallString& other) const noexcept
    {
        return std::strncmp(c_str(), other.c_str(), other.size()) == 0;
    }

private:
    [[nodiscard]] constexpr bool small() const noexcept { return tag(m_data) == small_tag; }

    [[nodiscard]] constexpr bool big() const noexcept { return tag(m_data) == big_tag; }

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

static_assert(sizeof(SmallString) == 8);

// NOLINTEND

#endif
