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
    TileType_DIRT_FLOOR,
    TileType_MARBLE_FLOOR,
    TileType_WALL,
    TileType_BORDER_WALL,
    TileType_STAIRS_DOWN,
    TileType_UNKNOWN_FLOOR,
    TileType_UNKNOWN_WALL,

    TileType_COUNT,
};

static const int final_dungeon_level = 5;
extern const int ethereal_radius;

extern int dungeon_level;
extern MapMatrix<TileType> actual_map_tiles;
extern MapMatrix<uint32_t> aesthetic_indexes;
extern Coord stairs_down_location;

void generate_map();

static inline bool is_in_bounds(Coord point) {
    return point.x >= 0 && point.x < map_size.x && point.y >= 0 && point.y < map_size.y;
}

static inline bool is_open_space(TileType tile_type) {
    switch (tile_type) {
        case TileType_UNKNOWN:
            // consider this open for the sake of path finding
            return true;
        case TileType_DIRT_FLOOR:
        case TileType_MARBLE_FLOOR:
        case TileType_UNKNOWN_FLOOR:
            return true;
        case TileType_WALL:
        case TileType_BORDER_WALL:
        case TileType_UNKNOWN_WALL:
            return false;
        case TileType_STAIRS_DOWN:
            return true;
        case TileType_COUNT:
            panic("not a tile type");
    }
    panic("tile type");
}
bool can_spawn_at(Coord away_from_location, Coord location);
bool is_open_line_of_sight(Coord from_location, Coord to_location, const MapMatrix<TileType> map_tiles);

Coord random_spawn_location(Coord away_from_location);
Coord find_stairs_down_location();

#endif
