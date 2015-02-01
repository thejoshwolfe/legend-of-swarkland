#include "util.hpp"

#include <stdio.h>
#include <stdlib.h>

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

void * panic_malloc(size_t count, size_t size) {
    void * mem = malloc(size * count);
    if (!mem)
        panic("out of memory");
    return mem;
}
