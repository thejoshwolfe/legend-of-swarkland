#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdint.h>
#include <stddef.h>

void panic(const char * str) __attribute__ ((noreturn));

void init_random();
uint32_t random_uint32();
static inline int random_int(int less_than_this) {
    return random_uint32() % less_than_this;
}
static inline int random_int(int at_least_this, int less_than_this) {
    return random_int(less_than_this - at_least_this) + at_least_this;
}

template <typename T>
static inline T min(T a, T b) {
    return a < b ? a : b;
}

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
static inline int euclidean_mod(int a, int base) {
    if (a < 0)
        return (a % base + base) % base;
    else
        return a % base;
}

template<typename T>
T * realloc_new(T * old_guy, size_t old_count, size_t new_count) {
    T * new_guy = new T[new_count];
    for (int i = 0; i < old_count; i++)
        new_guy[i] = old_guy[i];
    delete[] old_guy;
    return new_guy;
}

static inline int signf(float val) {
    if (val > 0) {
        return 1;
    } else if (val < 0) {
        return -1;
    } else {
        return 0;
    }
}

template<typename T, int(*Comparator)(T, T)>
void sort(T * in_place_list, int size) {
    // insertion sort, cuz whatever.
    for (int top = 1; top < size; top++) {
        T where_do_i_go = in_place_list[top];
        for (int falling_index = top - 1; falling_index >= 0; falling_index--){
            T do_you_want_my_spot = in_place_list[falling_index];
            if (Comparator(do_you_want_my_spot, where_do_i_go) <= 0) {
                // no one out of order here, officer
                break;
            }
            // these two are in the wrong order.
            // switch spots
            in_place_list[falling_index + 1] = do_you_want_my_spot;
            in_place_list[falling_index] = where_do_i_go;
        }
    }
}

#endif
