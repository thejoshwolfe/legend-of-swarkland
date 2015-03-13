#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

template<typename T>
class List {
public:
    List() {
    }
    ~List() {
        destroy(_items, _capacity);
    }

    T & operator[](int index) {
        bounds_check(index);
        return _items[index];
    }
    const T & operator[](int index) const {
        bounds_check(index);
        return _items[index];
    }
    int length() const {
        return _length;
    }

    int index_of(const T & item) {
        for (int i = 0; i < _length; i++)
            if (_items[i] == item)
                return i;
        return -1;
    }

    T * raw() {
        return _items;
    }
    const T * raw() const {
        return _items;
    }

    void append(const T & item) {
        ensure_capacity(_length + 1);
        _items[_length++] = item;
    }
    void append_all(const List<T> & items) {
        int other_length = items.length();
        ensure_capacity(_length + other_length);
        const T * other_raw = items.raw();
        for (int i = 0; i < other_length; i++)
            _items[_length + i] = other_raw[i];
        _length += other_length;
    }
    T pop() {
        if (_length == 0)
            panic("pop empty list");
        return _items[--_length];
    }

    void clear() {
        _length = 0;
    }
    void resize(int length) {
        if (length < 0)
            panic("negative resize");
        ensure_capacity(length);
        _length = length;
    }
    void remove_range(int start, int end) {
        if (!(0 <= start && start <= end && end <= _length))
            panic("bounds check");
        int delete_count = end - start;
        int move_count = min(delete_count, _length - end);
        for (int i = start; i < start + move_count; i++)
            _items[i] = _items[i + delete_count];
        _length -= delete_count;
    }

    bool operator==(const List<T> & other) const {
        if (this == &other)
            return true; // literally exactly the same thing
        if (_length != other._length)
            return false;
        for (int i = 0; i < _length; i++)
            if (_items[i] != other._items[i])
                return false;
        return true;
    }
    bool operator!=(const List<T> & other) const {
        return !(*this == other);
    }

private:
    T * _items = nullptr;
    int _length = 0;
    int _capacity = 0;

    void bounds_check(int index) const {
        if (index < 0 || index >= _length)
            panic("list index out of bounds");
    }
    void ensure_capacity(int new_capacity) {
        int better_capacity = _capacity;
        if (better_capacity == 0)
            better_capacity = 16;
        while (better_capacity < new_capacity)
            better_capacity *= 2;
        if (better_capacity != _capacity) {
            _items = reallocate(_items, _capacity, better_capacity);
            _capacity = better_capacity;
        }
    }

    List(const List<T> & other) = delete;
    List<T> & operator= (const List<T> & other) = delete;
};

#endif
