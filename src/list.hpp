#ifndef LIST_HPP
#define LIST_HPP

#include "util.hpp"

template<typename T>
class List {
public:
    List() {
        _length = 0;
        _capacity = 16;
        _items = new T[_capacity];
    }
    List(const List<T> & other) = delete;
    List<T>& operator= (const List<T> &other) = delete;

    ~List() {
        delete[] _items;
    }

    void append(T item) {
        ensure_capacity(_length + 1);
        _items[_length++] = item;
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
    T pop() {
        if (_length == 0)
            panic("pop empty list");
        return _items[--_length];
    }
    void clear() {
        _length = 0;
    }
    T * raw() {
        return _items;
    }
    const T * raw() const {
        return _items;
    }
    void resize(int length) {
        if (length < 0)
            panic("negative resize");
        ensure_capacity(length);
        _length = length;
    }

private:
    T * _items;
    int _length;
    int _capacity;

    void bounds_check(int index) const {
        if (index < 0 || index >= _length)
            panic("list index out of bounds");
    }
    void ensure_capacity(int new_capacity) {
        int better_capacity = _capacity;
        while (better_capacity < new_capacity)
            better_capacity *= 2;
        if (better_capacity != _capacity) {
            _items = realloc_new(_items, _capacity, better_capacity);
            _capacity = better_capacity;
        }
    }
};

#endif
