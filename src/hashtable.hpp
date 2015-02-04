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
static inline uint32_t hash_oversized(uint_oversized<Size64> a) {
    // it's just a bunch of xor
    uint64_t result = 0;
    for (int i = 0; i < Size64; i++)
        result ^= a.values[i];
    return (uint32_t)(result >> 32) ^ (uint32_t)(result & 0x00000000ffffffffULL);
}

template<int Size64>
static inline uint_oversized<Size64> random_oversized() {
    uint_oversized<Size64> result;
    for (int i = 0; i < Size64; i++)
        result.values[i] = ((uint64_t)random_uint32()) << 32 | (uint64_t)random_uint32();
    return result;
}

typedef uint_oversized<4> uint256;
uint32_t hash_uint256(uint256 a);
static inline uint256 random_uint256() {
    return random_oversized<4>();
}

// sorry. this isn't supposed to be visible.
template <typename K, typename V>
struct LinkedHashtableEntry {
    enum State {
        UNUSED = 0,
        VALID = 1,
        DELETED = 2,
    };
    K key;
    V value;
    State state;
    LinkedHashtableEntry * previous;
    LinkedHashtableEntry * next;
};

// A LinkedList and a Hashtable at the same time.
// iteration preserves insertion order.
template<typename K, typename V, uint32_t (*HashFunction)(K)>
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
        internal_put(key, value, false);
    }
    // if the key was already present, the value is updated to the specified value
    void put_or_overwrite(K key, V value) {
        internal_put(key, value, true);
    }
    // the key must be present.
    // it is safe to call this method to remove the item returned by an iterator, and then keep using the iterator.
    void remove(K key) {
        LinkedHashtableEntry<K, V> * entry = find_entry(key, false);
        entry->state = LinkedHashtableEntry<K, V>::DELETED;
        _size--;
        // join the two neighbors
        if (entry->previous != NULL) {
            entry->previous->next = entry->next;
        } else {
            _head = entry->next;
        }
        if (entry->next != NULL) {
            entry->next->previous = entry->previous;
        } else {
            _tail = entry->previous;
        }
        // leave the deleted entry's next pointer intact so that iterators can use it
    }
    // the key must be present
    V get(K key) const {
        return find_entry(key, false)->value;
    }
    // returns the default value if the key isn't found
    V get(K key, V default_value) const {
        LinkedHashtableEntry<K, V> * entry = find_entry(key, true);
        return entry != NULL ? entry->value : default_value;
    }
    bool contains(K key) const {
        return find_entry(key, true) != NULL;
    }
    class Iterator {
    public:
        bool has_next() {
            return cursor != NULL;
        }
        // you can remove this item from the underlying hashtable,
        // and this iterator will still work properly as long as you don't mutate the hashtable in any other way.
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
    uint32_t _size;
    uint32_t _capacity;
    LinkedHashtableEntry<K, V> * _table;
    LinkedHashtableEntry<K, V> * _head;
    LinkedHashtableEntry<K, V> * _tail;
    void init_capacity(uint32_t new_capacity) {
        _size = 0;
        _capacity = new_capacity;
        _table = new LinkedHashtableEntry<K, V>[_capacity];
        for (uint32_t i = 0; i < _capacity; i++)
            _table[i].state = LinkedHashtableEntry<K, V>::UNUSED;
        _head = NULL;
        _tail = NULL;
    }
    LinkedHashtableEntry<K, V> * find_entry(K key, bool tolerate_not_found) const {
        uint32_t start_index = HashFunction(key) % _capacity;
        for (uint32_t roll_over = 0; roll_over < _capacity; roll_over++) {
            uint32_t index = (start_index + roll_over) % _capacity;
            if (_table[index].state == LinkedHashtableEntry<K, V>::UNUSED)
                break; // no one has been here
            if (_table[index].state == LinkedHashtableEntry<K, V>::VALID && _table[index].key == key)
                return &_table[index];
        }
        if (!tolerate_not_found)
            panic("hashtable key not found");
        return NULL;
    }
    void internal_put(K key, V value, bool tolerate_collision) {
        just_put_one_thing(key, value, tolerate_collision);
        // if we get too full (75%), double the capacity
        if (_size * 4 >= _capacity * 3) {
            LinkedHashtableEntry<K, V> * old_table = _table;
            LinkedHashtableEntry<K, V> * old_head = _head;
            init_capacity(_capacity * 2);
            // load all the old elements into the new table in the proper order
            for (LinkedHashtableEntry<K, V> * cursor = old_head; cursor != NULL; cursor = cursor->next)
                just_put_one_thing(cursor->key, cursor->value, false);
            delete[] old_table;
        }
    }
    void just_put_one_thing(K key, V value, bool tolerate_collision) {
        uint32_t start_index = HashFunction(key) % _capacity;
        // resolve hash collisions with linear probing.
        for (uint32_t roll_over = 0; roll_over < _capacity; roll_over++) {
            uint32_t index = (start_index + roll_over) % _capacity;
            if (_table[index].state == LinkedHashtableEntry<K, V>::VALID && _table[index].key != key)
                continue; // overflow to the next slot
            // found where to put it
            if (_table[index].state == LinkedHashtableEntry<K, V>::VALID) {
                if (!tolerate_collision)
                    panic("hashtable put overwrites existing value");
                // overwrite existing entry
                _table[index].value = value;
            } else {
                // new addition
                _table[index].key = key;
                _table[index].value = value;
                _table[index].state = LinkedHashtableEntry<K, V>::VALID;
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
            }
            return;
        }
        // we're always supposed to maintain some space.
        panic("put into a full hashtable");
    }
};

template<typename T>
using IdMap = LinkedHashtable<uint256, T, hash_uint256>;

#endif
