#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "util.hpp"

struct Coord {
    int x;
    int y;
};
static inline bool operator==(Coord a, Coord b) {
    return a.x == b.x && a.y == b.y;
}
static inline bool operator!=(Coord a, Coord b) {
    return !(a == b);
}
static inline Coord operator+(Coord a, Coord b) {
    return {b.x + a.x, b.y + a.y};
}
static inline Coord operator-(Coord a, Coord b) {
    return {a.x - b.x, a.y - b.y};
}

template<typename T>
class Matrix {
public:
    Matrix(Coord size) :
            _size(size) {
        items = new T[size.x * size.y];
    }
    Matrix(Matrix &) = delete;
    ~Matrix() {
        delete[] items;
    }
    T & operator[](Coord index) {
        if (index.x < 0 || index.x >= _size.x || index.y < 0 || index.y >= _size.y)
            panic("matrix bounds check");
        return items[index.y * _size.x + index.x];
    }
    void set_all(T value) {
        for (int i = 0; i < _size.x * _size.y; i++) {
            items[i] = value;
        }
    }
private:
    Coord _size;
    T * items;
};

static inline int distance_squared(Coord a, Coord b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    return dx * dx + dy * dy;
}

// each of x and y will be -1, 0, 1
static inline Coord sign(Coord value) {
    return {sign(value.x), sign(value.y)};
}

static inline Coord clamp(Coord value, Coord min, Coord max) {
    return {clamp(value.x, min.x, max.x), clamp(value.y, min.y, max.y)};
}

#endif
