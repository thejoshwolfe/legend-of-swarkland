#ifndef UTIL_HPP
#define UTIL_HPP

#include <stddef.h>

void panic(const char * str) __attribute__ ((noreturn));

static inline int clamp(int value, int min, int max) {
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

static inline int sign(int value) {
    if (value == 0)
        return 0;
    return value < 0 ? -1 : 1;
}

template<typename T>
T * realloc_new(T * old_guy, size_t old_count, size_t new_count) {
    T * new_guy = new T[new_count];
    for (int i = 0; i < old_count; i++)
        new_guy[i] = old_guy[i];
    delete[] old_guy;
    return new_guy;
}

#endif
