#ifndef PTR_TAG_H
#define PTR_TAG_H

#include <bit>
#include <cassert>
#include <cstddef>

// Pointer tagging.
// Since malloc's returned address is guaranteed to be aligned at least as std::max_align_t, we will
// use unused last bits in pointer to store additonal info. This can be used to simulate base class
// and virtual method calls.
//
static constexpr uintptr_t tag_bits = alignof(std::max_align_t) - 1;

constexpr uintptr_t raw(const void* ptr) noexcept
{
    return std::bit_cast<uintptr_t>(ptr);
}

constexpr uintptr_t tag(const void* ptr) noexcept
{
    return raw(ptr) & tag_bits;
}

constexpr void* clear_tag(const void* ptr) noexcept
{
    return std::bit_cast<void*>(raw(ptr) & ~tag_bits);
}

constexpr void* set_tag(const void* ptr, uintptr_t tag) noexcept
{
    assert((tag & ~tag_bits) == 0);
    return std::bit_cast<void*>(raw(clear_tag(ptr)) | (tag & tag_bits));
}

#endif // PTR_TAG_H
