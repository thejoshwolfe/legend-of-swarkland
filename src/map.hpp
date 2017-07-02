#ifndef MAP_HPP
#define MAP_HPP

#include "geometry.hpp"

#include <stdlib.h>

enum {
    MAP_SIZE_X = 50,
    MAP_SIZE_Y = 25,
};
static constexpr Coord map_size = { MAP_SIZE_X, MAP_SIZE_Y };

template <typename T>
using MapMatrix = Matrix<T, MAP_SIZE_X, MAP_SIZE_Y>;

enum TileType {
    TileType_UNKNOWN,
    TileType_DIRT_FLOOR,
    TileType_MARBLE_FLOOR,
    TileType_LAVA_FLOOR,
    TileType_BROWN_BRICK_WALL,
    TileType_GRAY_BRICK_WALL,
    TileType_BORDER_WALL,
    TileType_STAIRS_DOWN,
    TileType_UNKNOWN_FLOOR,
    TileType_UNKNOWN_WALL,

    TileType_COUNT,
};

static const int final_dungeon_level = 5;
static const int ethereal_radius = 5;

void animate_map_tiles();
void generate_map();

static inline bool is_in_bounds(Coord point) {
    return point.x >= 0 && point.x < map_size.x && point.y >= 0 && point.y < map_size.y;
}

static inline bool is_open_space(TileType tile_type) {
    switch (tile_type) {
        case TileType_UNKNOWN:
            // not sure what to say here.
            return true;
        case TileType_DIRT_FLOOR:
        case TileType_MARBLE_FLOOR:
        case TileType_LAVA_FLOOR:
        case TileType_UNKNOWN_FLOOR:
            return true;
        case TileType_BROWN_BRICK_WALL:
        case TileType_GRAY_BRICK_WALL:
        case TileType_BORDER_WALL:
        case TileType_UNKNOWN_WALL:
            return false;
        case TileType_STAIRS_DOWN:
            return true;
        case TileType_COUNT:
            unreachable();
    }
    unreachable();
}
static inline bool is_safe_space(TileType tile_type) {
    switch (tile_type) {
        case TileType_UNKNOWN:
            // consider this open for the sake of path finding
            return true;
        case TileType_DIRT_FLOOR:
        case TileType_MARBLE_FLOOR:
        case TileType_UNKNOWN_FLOOR:
            return true;
        case TileType_LAVA_FLOOR:
        case TileType_BROWN_BRICK_WALL:
        case TileType_GRAY_BRICK_WALL:
        case TileType_BORDER_WALL:
        case TileType_UNKNOWN_WALL:
            return false;
        case TileType_STAIRS_DOWN:
            return true;
        case TileType_COUNT:
            unreachable();
    }
    unreachable();
}
static inline bool is_diggable_wall(TileType tile_type) {
    switch (tile_type) {
        case TileType_BROWN_BRICK_WALL:
        case TileType_GRAY_BRICK_WALL:
            return true;
        case TileType_UNKNOWN:
        case TileType_DIRT_FLOOR:
        case TileType_MARBLE_FLOOR:
        case TileType_LAVA_FLOOR:
        case TileType_UNKNOWN_FLOOR:
        case TileType_BORDER_WALL:
        case TileType_UNKNOWN_WALL:
        case TileType_STAIRS_DOWN:
            return false;
        case TileType_COUNT:
            unreachable();
    }
    unreachable();
}
bool is_open_line_of_sight(Coord from_location, Coord to_location, const MapMatrix<TileType> & map_tiles);

void record_shape_of_terrain(MapMatrix<TileType> * tiles, Coord location);
Coord random_spawn_location(Coord away_from_location);

#endif
