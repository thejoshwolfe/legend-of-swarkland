#ifndef MAP_HPP
#define MAP_HPP

#include "geometry.hpp"

#include <stdlib.h>

enum {
    MAP_SIZE_X = 50,
    MAP_SIZE_Y = 25,
};
static const Coord map_size = { MAP_SIZE_X, MAP_SIZE_Y };

template <typename T>
using MapMatrix = Matrix<T, MAP_SIZE_X, MAP_SIZE_Y>;

enum TileType {
    TileType_UNKNOWN,
    TileType_FLOOR,
    TileType_WALL,
    TileType_BORDER_WALL,
    TileType_STAIRS_DOWN,

    TileType_COUNT,
};

struct Tile {
    TileType tile_type;
    int aesthetic_index;
};

static const Tile unknown_tile = {TileType_UNKNOWN, 0};
static const int final_dungeon_level = 5;

extern int dungeon_level;
extern MapMatrix<Tile> actual_map_tiles;
extern Coord stairs_down_location;

void generate_map();

// TODO: this function shouldn't really be necessary now that oob is protected by border walls
static inline bool is_in_bounds(Coord point) {
    return point.x >= 0 && point.x < map_size.x && point.y >= 0 && point.y < map_size.y;
}

static inline bool is_open_space(TileType tile_type) {
    return tile_type == TileType_FLOOR || tile_type == TileType_STAIRS_DOWN;
}

#endif
