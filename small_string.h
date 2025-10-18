#ifndef SMALL_STRING_H
#define SMALL_STRING_H

#include <cstring>
#include <string>

#include "ptr_tag.h"
#include "types.h"

// NOLINTBEGIN

// Small string.
// If string is smaller than 7 bytes, data will be stored in pointer directly, otherwise m_data will
// point to allocated string. We will set pointer tag (small or big) which indicates whether string
// is small or big.
//
class SmallString {
    static constexpr sz small_limit = 6;
    static constexpr uptr small_tag = 0;
    static constexpr uptr big_tag = 1;

public:
    static constexpr sz npos = -1;

    constexpr SmallString() : m_data{nullptr} {}

    constexpr SmallString(const std::string& s) : SmallString(s.c_str(), s.size()) {}

    constexpr SmallString(const char* s, sz size = 0) : m_data{nullptr}
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

        free_buffer(big_data());
    }

    /**
     * Allocates buffer long enough to hold size bytes.
     * It allocates memory from pools of 1MB. When chunk is exceeded, new chunk is requested with
     * malloc. Allocations are aligned on a std::max_align_t (usually 16) bytes.
     * Note that this function is not thread safe.
     */
    static void* allocate_buffer(sz size)
    {
        static constexpr sz align = alignof(std::max_align_t);
        static constexpr sz align_mask = align - 1;
        static constexpr sz chunk_size = 1024UL * 1024;

        static u8* m_memory = nullptr;
        static sz m_allocated = 0;

        sz aligned_size = (size + align_mask) & ~align_mask;

        if (m_memory == nullptr || m_allocated + aligned_size > chunk_size) {
            m_memory = static_cast<u8*>(std::malloc(chunk_size));
            m_allocated = 0;
        }

        void* allocation = m_memory + m_allocated;
        m_allocated += aligned_size;

        return allocation;
    }

    /**
     * Nothing to do in delete.
     */
    static void free_buffer([[maybe_unused]] void* memory) {}

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

    [[nodiscard]] constexpr void* big_data() const noexcept { return clear_tag(m_data); }

    [[nodiscard]] constexpr const char* c_str() const noexcept
    {
        return small() ? &m_sso[1] : static_cast<char*>(clear_tag(m_data));
    }

    [[nodiscard]] constexpr std::string str() const noexcept { return std::string{c_str()}; }

    [[nodiscard]] constexpr operator const char*() const noexcept { return c_str(); }

    [[nodiscard]] constexpr operator bool() const noexcept { return !empty(); }

    [[nodiscard]] sz size() const noexcept { return std::strlen(*this); }

    [[nodiscard]] bool starts_with(const SmallString& other) const noexcept
    {
        return std::strncmp(c_str(), other.c_str(), other.size()) == 0;
    }

    [[nodiscard]] bool contains(const char* needle) const noexcept
    {
        return std::strstr(c_str(), needle);
    }

    [[nodiscard]] bool contains(const std::string& needle) const noexcept
    {
        return contains(needle.c_str());
    }

    [[nodiscard]] sz find(const char* needle, sz offset = 0) const noexcept
    {
        assert(offset <= size()); // we are always 0 terminated.

        const char* r = std::strstr(c_str() + offset, needle);
        return r != nullptr ? r - c_str() : npos;
    }

    [[nodiscard]] sz find(const std::string& needle, sz offset = 0) const noexcept
    {
        return find(needle.c_str(), offset);
    }

    [[nodiscard]] sz find(const std::string_view& needle, sz offset = 0) const noexcept
    {
        return find(needle.data(), offset);
    }

private:
    [[nodiscard]] constexpr bool small() const noexcept { return tag(m_data) == small_tag; }

    [[nodiscard]] constexpr bool big() const noexcept { return tag(m_data) == big_tag; }

    void ctr_from_big(const char* big, sz size)
    {
        assert(size > small_limit);

        char* str = static_cast<char*>(allocate_buffer(size + 1));
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
