#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "util.hpp"

struct Coord {
    int x;
    int y;
    static inline Coord nowhere() {
        return {-1, -1};
    }
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
static inline void operator+=(Coord & a, Coord b) {
    a = a + b;
}
static inline void operator-=(Coord & a, Coord b) {
    a = a - b;
}

template<typename T, int SizeX, int SizeY>
class Matrix {
public:
    T & operator[](Coord index) {
        if (index.x < 0 || index.x >= SizeX || index.y < 0 || index.y >= SizeY)
            panic("matrix bounds check");
        return items[index.y * SizeX + index.x];
    }
    void set_all(const T & value) {
        for (int i = 0; i < SizeX * SizeY; i++)
            items[i] = value;
    }
private:
    T items[SizeX * SizeY];
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
