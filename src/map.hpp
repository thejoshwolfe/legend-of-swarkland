#ifndef MAP_HPP
#define MAP_HPP

#include "geometry.hpp"

#include <stdlib.h>

static const Coord map_size = { 50, 25 };

enum TileType {
    TileType_UNKNOWN,
    TileType_FLOOR,
    TileType_WALL,

    TileType_COUNT,
};

struct Tile {
    TileType tile_type;
    int aesthetic_index;
};

static const Tile unknown_tile = {TileType_UNKNOWN, 0};

extern Matrix<Tile> actual_map_tiles;

void generate_map();

static inline bool is_in_bounds(Coord point) {
    return point.y >= 0 && point.y < map_size.y && point.x >= 0 && point.x < map_size.x;
}

#endif
