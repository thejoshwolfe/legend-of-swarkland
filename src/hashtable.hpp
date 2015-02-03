#ifndef HASHTABLE_HPP
#define HASHTABLE_HPP

#include "util.hpp"

#include <stdint.h>

template<int Size64>
struct uint_oversized {
    uint64_t values[Size64];
};

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

typedef uint_oversized<4> uint256;
int hash_uint256(uint256 a);
static inline uint256 random_uint256() {
    return random_oversized<4>();
}

// sorry. this isn't supposed to be visible.
template <typename K, typename V>
struct LinkedHashtableEntry {
    K key;
    V value;
    bool is_valid;
    LinkedHashtableEntry * previous;
    LinkedHashtableEntry * next;
};

// A LinkedList and a Hashtable at the same time.
// iteration preserves insertion order.
template<typename K, typename V, int (*HashFunction)(K)>
class LinkedHashtable {
public:
    LinkedHashtable() {
        init_capacity(16);
    }
    ~LinkedHashtable() {
        delete[] _table;
    }
    int size() const {
        return _size;
    }
    // the key must not already be present
    void put(K key, V value) {
        just_put(key, value);
        // if we get too full (75%), double the capacity
        if (_size * 4 >= _capacity * 3) {
            LinkedHashtableEntry<K, V> * old_table = _table;
            LinkedHashtableEntry<K, V> * old_head = _head;
            init_capacity(_capacity * 2);
            // load all the old elements into the new table in the proper order
            for (LinkedHashtableEntry<K, V> * cursor = old_head; cursor != NULL; cursor = cursor->next)
                just_put(cursor->key, cursor->value);
            delete[] old_table;
        }
    }
    // the key must be present
    void remove(K key) {
        LinkedHashtableEntry<K, V> & entry = find_entry(key);
        entry.is_valid = false;
        _size--;
        // join the two neighbors
        if (entry.previous != NULL) {
            entry.previous->next = entry.next;
        } else {
            _head = entry.next;
        }
        if (entry.next != NULL) {
            entry.next->previous = entry.previous;
        } else {
            _tail = entry.previous;
        }
    }
    // the key must be present
    V get(K key) {
        return find_entry(key).value;
    }
    class Iterator {
    public:
        bool has_next() {
            return cursor != NULL;
        }
        V next() {
            V result = cursor->value;
            cursor = cursor->next;
            return result;
        }
    private:
        LinkedHashtableEntry<K, V> * cursor;
        Iterator(LinkedHashtableEntry<K, V> * cursor) :
                cursor(cursor) {
        }
        friend LinkedHashtable;
    };
    Iterator value_iterator() {
        return Iterator(_head);
    }
private:
    int _size;
    int _capacity;
    LinkedHashtableEntry<K, V> * _table;
    LinkedHashtableEntry<K, V> * _head;
    LinkedHashtableEntry<K, V> * _tail;
    void init_capacity(int new_capacity) {
        _size = 0;
        _capacity = new_capacity;
        _table = new LinkedHashtableEntry<K, V>[_capacity];
        for (int i = 0; i < _capacity; i++)
            _table[i].is_valid = false;
        _head = NULL;
        _tail = NULL;
    }
    LinkedHashtableEntry<K, V> & find_entry(K key) {
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
        // resolve hash collisions with linear probing.
        for (int roll_over = 0; roll_over < _capacity; roll_over++) {
            int index = (start_index + roll_over) % _capacity;
            if (_table[index].is_valid && _table[index].key != key)
                continue;
            // found where to put it
            if (_table[index].is_valid) {
                panic("hashtable put overwrites existing value");
            }
            _table[index] = {key, value, true};
            _size++;
            if (_tail == NULL) {
                _head = &_table[index];
                _table[index].previous = NULL;
            } else {
                _tail->next = &_table[index];
                _table[index].previous = _tail;
            }
            _table[index].next = NULL;
            _tail = &_table[index];
            return;
        }
        // we're always supposed to maintain some space.
        panic("put into a full hashtable");
    }
};

#endif
