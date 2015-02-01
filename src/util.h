#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

void panic(const char * str) __attribute__ ((noreturn));
void * panic_malloc(size_t count, size_t size);
static inline int clamp(int value, int min, int max) {
    if (value < min)
        return min;
    if (value > max)
        return max;
    return value;
}

#endif
