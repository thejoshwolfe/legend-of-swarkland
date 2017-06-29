#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

#include "util.hpp"

struct Coord {
    int x;
    int y;
    static constexpr Coord nowhere() {
        return {-1, -1};
    }
};
static constexpr bool operator==(Coord a, Coord b) {
    return a.x == b.x && a.y == b.y;
}
static constexpr bool operator!=(Coord a, Coord b) {
    return !(a == b);
}
static constexpr Coord operator+(Coord a, Coord b) {
    return {b.x + a.x, b.y + a.y};
}
static constexpr Coord operator-(Coord a, Coord b) {
    return {a.x - b.x, a.y - b.y};
}
static constexpr void operator+=(Coord & a, Coord b) {
    a = a + b;
}
static constexpr void operator-=(Coord & a, Coord b) {
    a = a - b;
}

// each of x and y will be -1, 0, 1
static constexpr Coord sign(Coord value) {
    return {sign(value.x), sign(value.y)};
}
static constexpr Coord clamp(Coord value, Coord min, Coord max) {
    return {clamp(value.x, min.x, max.x), clamp(value.y, min.y, max.y)};
}
static constexpr Coord abs(Coord value) {
    return {abs(value.x), abs(value.y)};
}
// r^2 = dx^2 + dy^2
static constexpr int euclidean_distance_squared(Coord a, Coord b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    return dx * dx + dy * dy;
}
// how many moves does it take, assuming you can move diagonally
static constexpr int ordinal_distance(Coord a, Coord b) {
    Coord abs_vector = abs(b - a);
    return max(abs_vector.x, abs_vector.y);
}
// how many moves does it take, assuming you cannot move diagonally
static constexpr int cardinal_distance(Coord a, Coord b) {
    Coord abs_vector = abs(b - a);
    return abs_vector.x + abs_vector.y;
}
static constexpr int compare(const Coord & a, const Coord & b) {
    int result = 0;
    if ((result = compare(a.x, b.x)) != 0)
        return result;
    if ((result = compare(a.y, b.y)) != 0)
        return result;
    return 0;
}

struct Rect {
    Coord position;
    Coord size;
    static constexpr Rect nowhere() {
        return {Coord::nowhere(), Coord::nowhere()};
    }
};
static constexpr bool operator==(Rect a, Rect b) {
    return a.position == b.position && a.size == b.size;
}
static constexpr bool operator!=(Rect a, Rect b) {
    return !(a == b);
}

template<typename T, int SizeX, int SizeY>
class Matrix {
public:
    T & operator[](Coord index) {
        if (index.x < 0 || index.x >= SizeX || index.y < 0 || index.y >= SizeY)
            panic("matrix bounds check");
        return items[index.y * SizeX + index.x];
    }
    const T & operator[](Coord index) const {
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

#endif
