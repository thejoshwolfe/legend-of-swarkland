#ifndef UINT_OVERSIZED_HPP
#define UINT_OVERSIZED_HPP

#include "util.hpp"

#include <stdint.h>

template<size_t Size64>
struct uint_oversized {
    uint64_t values[Size64];

    static constexpr uint_oversized<Size64> zero() {
        return {0};
    }
};

DEFINE_GDB_PY_SCRIPT("debug-scripts/uint_oversized.py")

// comparison
template<size_t Size64>
static constexpr bool operator==(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    for (size_t i = 0; i < Size64; i++)
        if (a.values[i] != b.values[i])
            return false;
    return true;
}
template<size_t Size64>
static constexpr bool operator!=(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    return !(a == b);
}
template<size_t Size64>
static constexpr int compare(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    for (size_t i = 0; i < Size64; i++) {
        if (a.values[i] == b.values[i])
            continue;
        return a.values[i] < b.values[i] ? -1 : 1;
    }
    return 0;
}

// bitwise arithmetic
template<size_t Size64>
static constexpr uint_oversized<Size64> operator|(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    uint_oversized<Size64> result;
    for (size_t i = 0; i < Size64; i++)
        result[i] = a.values[i] | b.values[i];
    return result;
}
template<size_t Size64>
static constexpr uint_oversized<Size64> operator&(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    uint_oversized<Size64> result;
    for (size_t i = 0; i < Size64; i++)
        result[i] = a.values[i] & b.values[i];
    return result;
}
template<size_t Size64>
static constexpr uint_oversized<Size64> operator^(const uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    uint_oversized<Size64> result;
    for (size_t i = 0; i < Size64; i++)
        result[i] = a.values[i] ^ b.values[i];
    return result;
}

template<size_t Size64>
static constexpr uint_oversized<Size64> & operator|=(uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    for (size_t i = 0; i < Size64; i++)
        a.values[i] |= b.values[i];
    return a;
}
template<size_t Size64>
static constexpr uint_oversized<Size64> & operator&=(uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    for (size_t i = 0; i < Size64; i++)
        a.values[i] &= b.values[i];
    return a;
}
template<size_t Size64>
static constexpr uint_oversized<Size64> & operator^=(uint_oversized<Size64> & a, const uint_oversized<Size64> & b) {
    for (size_t i = 0; i < Size64; i++)
        a.values[i] ^= b.values[i];
    return a;
}

template<size_t Size64>
static constexpr uint_oversized<Size64> operator~(const uint_oversized<Size64> & a) {
    uint_oversized<Size64> result;
    for (size_t i = 0; i < Size64; i++)
        result[i] = ~a.values[i];
    return result;
}

// more fun stuff
template<size_t Size64>
static constexpr int ctz(const uint_oversized<Size64> & a) {
    int result = 0;
    for (size_t i = 0; i < Size64; i++) {
        if (a[i] == 0)
            result += 64;
        else
            return result + ctz(a[i]);
    }
    return result;
}

#endif
