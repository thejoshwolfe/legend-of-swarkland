#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "util.hpp"

struct Coord {
    int x;
    int y;
    Coord() :
            x(0), y(0) {
    }
    Coord(int x, int y) :
            x(x), y(y) {
    }
};
static inline bool operator==(const Coord & a, const Coord & b) {
    return a.x == b.x && a.y == b.y;
}
static inline bool operator!=(const Coord & a, const Coord & b) {
    return !(a == b);
}

template<typename T>
class Matrix {
public:
    Matrix(int height, int width) :
            _height(height), _width(width) {
        items = new T[height * width];
    }
    Matrix(Matrix &) = delete;
    ~Matrix() {
        delete[] items;
    }
    T & operator[](Coord index) {
        if (index.x < 0 || index.x >= _width || index.y < 0 || index.y >= _height)
            panic("matrix bounds check");
        return items[index.y * _width + index.x];
    }
    void setAll(T value) {
        for (int i = 0; i < _height * _width; i++)
            items[i] = value;
    }
private:
    T * items;
    int _height;
    int _width;
};

#endif
