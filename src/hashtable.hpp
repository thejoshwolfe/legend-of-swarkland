#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "util.hpp"

#include <stdint.h>

template<int Size64>
struct uint_oversized {
    uint64_t values[Size64];
};
typedef uint_oversized<4> uint256;

template<int Size64>
static inline bool operator==(uint_oversized<Size64> a, uint_oversized<Size64> b) {
    for (int i = 0; i < Size64; i++)
        if (a.values[i] != b.values[i])
            return false;
    return true;
}
template<int Size64>
static inline bool operator!=(uint_oversized<Size64> a, uint_oversized<Size64> b) {
    return !(a == b);
}
template<int Size64>
static inline int hash_oversized(uint_oversized<Size64> a) {
    // it's just a bunch of xor
    int result = 0;
    for (int i = 0; i < Size64; i++)
        result ^= (int)(a.values[i] >> 32) ^ (int)(a.values[i] & 0x00000000ffffffffULL);
    return result;
}

template<int Size64>
static inline uint_oversized<Size64> random_oversized() {
    uint_oversized<Size64> result;
    for (int i = 0; i < Size64; i++) {
        result.values[i] = ((uint64_t)random_int(0x10000)) << 48 |
                           ((uint64_t)random_int(0x10000)) << 32 |
                           ((uint64_t)random_int(0x10000)) << 16 |
                           ((uint64_t)random_int(0x10000)) << 0;
    }
    return result;
}

// uses linear probing
template<typename K, typename V, int (*HashFunction)(K)>
class Hashtable {
public:
    Hashtable() {
        init_capacity(16);
    }
    ~Hashtable() {
        delete[] _table;
    }
    int size() const {
        return _size;
    }
    void put(K key, V value) {
        just_put(key, value);
        // if we get too full (75%), double the capacity
        if (_size * 4 >= _capacity * 3) {
            Entry * old_table = _table;
            int old_capacity = _capacity;
            init_capacity(_capacity * 2);
            // dump all the old elements into the new table
            for (int i = 0; i < old_capacity; i++) {
                if (old_table[i].is_valid)
                    just_put(old_table[i].key, old_table[i].value);
            }
            delete[] old_table;
        }
    }
    void remove(K key) {
        find_entry(key).is_valid = false;
        _size--;
    }
    V get(K key) {
        return find_entry(key).value;
    }
private:
    struct Entry {
        K key;
        V value;
        bool is_valid;
    };
    int _size;
    int _capacity;
    Entry * _table;
    void init_capacity(int new_capacity) {
        _size = 0;
        _capacity = new_capacity;
        _table = new Entry[_capacity];
        for (int i = 0; i < _capacity; i++)
            _table[i].is_valid = false;
    }
    Entry & find_entry(K key) {
        int start_index = HashFunction(key) % _capacity;
        for (int roll_over = 0; roll_over < _capacity; roll_over++) {
            int index = (start_index + roll_over) % _capacity;
            if (_table[index].is_valid && _table[index].key == key)
                return _table[index];
        }
        panic("hashtable key not found");
    }
    void just_put(K key, V value) {
        int start_index = HashFunction(key) % _capacity;
        for (int roll_over = 0; roll_over < _capacity; roll_over++) {
            int index = (start_index + roll_over) % _capacity;
            if (_table[index].is_valid && _table[index].key != key)
                continue;
            // found where to put it
            if (_table[index].is_valid) {
                // we're overwriting an old value with the same key
            } else {
                // we're adding an entry
                _size++;
            }
            _table[index] = {key, value, true};
            return;
        }
        panic("put into a full hashtable");
    }
};

#endif
