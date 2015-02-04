#include "util.hpp"

#include "random.hpp"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

static RandomState the_random_state;

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

void init_random() {
    uint32_t seed = time(NULL);
    init_random_state(&the_random_state, seed);
}

uint32_t random_uint32() {
    return get_random(&the_random_state);
}
