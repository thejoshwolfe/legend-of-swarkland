#ifndef UTIL_HPP
#define UTIL_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <new>

void panic(const char * str) __attribute__ ((noreturn));

// create<MyClass>(a, b) is equivalent to: new MyClass(a, b)
template<typename T, typename... Args>
static inline T * create(Args... args) {
    T * ptr = reinterpret_cast<T*>(malloc(sizeof(T)));
    if (!ptr)
        panic("allocate: out of memory");
    new (ptr) T(args...);
    return ptr;
}

// allocate<MyClass>(10) is equivalent to: new MyClass[10]
// calls the default constructor for each item in the array.
template<typename T>
static inline T * allocate(size_t count) {
    T * ptr = reinterpret_cast<T*>(malloc(count * sizeof(T)));
    if (!ptr)
        panic("allocate: out of memory");
    for (size_t i = 0; i < count; i++)
        new (&ptr[i]) T;
    return ptr;
}

// Pass in a pointer to an array of old_count items.
// You will get a pointer to an array of new_count items
// where the first old_count items will have the same bits as the array you passed in.
// Calls the default constructor on all the new elements.
// The returned pointer may not be equal to the given pointer,
// and no methods are called in the event of copying/moving the bits to a new buffer
// (no default constructor, no assignment operator, and no destructor, as you would expect from a manual implementation).
// This means you can't count on any pointers to these elements remaining valid after this call.
// If new_count is less than old_count, i.e. this is a shrinking operation, behavior is undefined.
template<typename T>
static inline T * reallocate(T * old, size_t old_count, size_t new_count) {
    T * new_ptr = reinterpret_cast<T*>(realloc(old, new_count * sizeof(T)));
    if (!new_ptr)
        panic("reallocate: out of memory");
    for (size_t i = old_count; i < new_count; i++)
        new (&new_ptr[i]) T;
    return new_ptr;
}

// calls destructors and frees the memory.
// the count parameter is only used to call destructors of array elements.
// provide a count of 1 if this is not an array,
// or a count of 0 to skip the destructors.
template<typename T>
static inline void destroy(T * ptr, size_t count) {
    for (size_t i = 0; i < count; i++)
        ptr[i].T::~T();
    free(ptr);
}

void init_random();
uint32_t random_uint32();
static inline int random_int(int less_than_this) {
    return random_uint32() % less_than_this;
}
static inline int random_int(int at_least_this, int less_than_this) {
    return random_int(less_than_this - at_least_this) + at_least_this;
}
static inline int random_inclusive(int min, int max) {
    return random_int(min, max + 1);
}

template <typename T>
static inline T min(T a, T b) {
    return a < b ? a : b;
}
template <typename T>
static inline T max(T a, T b) {
    return a < b ? b : a;
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

template<typename T>
void shuffle(T * in_place_list, int size) {
    for (int i = 0; i < size; i++) {
        int swap_with = random_int(i, size);
        T tmp = in_place_list[swap_with];
        in_place_list[swap_with] = in_place_list[i];
        in_place_list[i] = tmp;
    }
}

template<typename InnerIterator, typename ValueType>
class FilteredIterator {
public:
    bool next(ValueType * output) {
        while (iterator.next(output))
            if (filter(*output))
                return true;
        return false;
    }
    FilteredIterator(const InnerIterator & hashtable, bool (*filter)(ValueType)) :
            iterator(hashtable), filter(filter) {
    }
private:
    InnerIterator iterator;
    bool (*filter)(ValueType);
};

#endif
