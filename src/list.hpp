#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

template<typename T>
class List {
public:
    List() {
        _size = 0;
        _capacity = 16;
        _items = new T[_capacity];
    }
    ~List() {
        delete[] _items;
    }

    void add(T item) {
        ensure_capacity(_size + 1);
        _items[_size++] = item;
    }
    T & operator[](int index) {
        bounds_check(index);
        return _items[index];
    }
    const T & operator[](int index) const {
        bounds_check(index);
        return _items[index];
    }
    int size() const {
        return _size;
    }
    T pop() {
        if (_size == 0)
            panic("pop empty list");
        return _items[--_size];
    }
    void clear() {
        _size = 0;
    }

private:
    T * _items;
    int _size;
    int _capacity;

    void ensure_capacity(int new_capacity) {
        int better_capacity = _capacity;
        while (better_capacity < new_capacity)
            better_capacity *= 2;
        if (better_capacity != _capacity) {
            _items = realloc_new(_items, _capacity, better_capacity);
            _capacity = better_capacity;
        }
    }
    void bounds_check(int index) const {
        if (index < 0 || index >= _size)
            panic("list index out of bounds");
    }
};

#endif
