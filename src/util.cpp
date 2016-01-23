#include "util.hpp"

#include "random.hpp"
#include "tas.hpp"

#include <stdio.h>
#include <stdlib.h>

static RandomState the_random_state;

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

void init_random() {
    // no RNG in test mode
    if (test_mode)
        return;
    uint32_t seed = tas_get_seed();
    init_random_state(&the_random_state, seed);
}

uint32_t random_uint32() {
    if (test_mode)
        panic("RNG not allowed in test mode");
    return get_random(&the_random_state);
}

static int get_rng_input(const char * type_str, int value, const char * comment) {
    ByteBuffer tag;
    tag.format("%s:%d:%s", type_str, value, comment);
    return tas_get_rng_input(tag);
}

int random_midpoint(int midpoint, const char * comment) {
    if (test_mode)
        return get_rng_input("midpoint", midpoint, comment);
    return random_inclusive(midpoint * 2 / 3, midpoint * 3 / 2);
}


int log2(int value) {
    if (value < 0)
        return 0;
    int result = 0;
    while (value >> result > 1)
        result++;
    return result;
}
