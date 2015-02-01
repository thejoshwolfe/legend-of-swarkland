#ifndef GEOMETRY_HPP
#define GEOMETRY_HPP

struct Coord {
    int x;
    int y;
    Coord(int x, int y) :
            x(x), y(y) {
    }
};

static inline bool operator==(const Coord & a, const Coord & b) {
    return a.x == b.x && a.y == b.y;
}

#endif
