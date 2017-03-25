#include <serial.hpp>
#include "util.hpp"

#include "random.hpp"
#include <stdio.h>
#include <stdlib.h>

void _panic(const char * file_name, int line_number, const char * str, const char * str2) {
    if (str2 != nullptr) {
        fprintf(stderr, "%s:%d: panic: %s: %s\n", file_name, line_number, str, str2);
    } else {
        fprintf(stderr, "%s:%d: panic: %s\n", file_name, line_number, str);
    }
    abort();
}

uint32_t random_uint32() {
    if (game->test_mode)
        panic("RNG not allowed in test mode");
    return get_random(&game->the_random_state);
}

static int get_rng_input(const char * type_str, int value, const char * comment) {
    ByteBuffer tag;
    tag.format("%s:%d:%s", type_str, value, comment);
    return read_rng_input_from_save_file(tag);
}
static int get_rng_input(const char * type_str, int value1, int value2, const char * comment) {
    ByteBuffer tag;
    tag.format("%s:%d:%d:%s", type_str, value1, value2, comment);
    return read_rng_input_from_save_file(tag);
}

int random_int(int less_than_this, const char * comment) {
    if (comment != nullptr && game->test_mode)
        return get_rng_input("int", less_than_this, comment);
    return random_uint32() % less_than_this;
}
int random_int(int at_least_this, int less_than_this, const char * comment) {
    if (comment != nullptr && game->test_mode)
        return get_rng_input("range", at_least_this, less_than_this, comment);
    return random_int(less_than_this - at_least_this, nullptr) + at_least_this;
}
int random_inclusive(int min, int max, const char * comment) {
    if (comment != nullptr && game->test_mode)
        return get_rng_input("inclusive", min, max, comment);
    return random_int(min, max + 1, nullptr);
}
int random_midpoint(int midpoint, const char * comment) {
    if (comment != nullptr && game->test_mode)
        return get_rng_input("midpoint", midpoint, comment);
    return random_inclusive(midpoint * 2 / 3, midpoint * 3 / 2, nullptr);
}


int log2(int value) {
    if (value < 0)
        return 0;
    int result = 0;
    while (value >> result > 1)
        result++;
    return result;
}
