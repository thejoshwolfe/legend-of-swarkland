#ifndef MAP_HPP
#define MAP_HPP

#include "geometry.hpp"
#include "vision_types.hpp"

#include <stdlib.h>

static const Coord map_size = { 55, 30 };

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

class Knowledge {
public:
    Matrix<Tile> tiles;
    Matrix<VisionTypes> is_visible;
    Knowledge() :
            tiles(map_size.y, map_size.x), is_visible(map_size.y, map_size.x) {
        tiles.set_all(Tile());
        is_visible.set_all({0, 0});
    }
};

extern Matrix<Tile> actual_map_tiles;

struct Individual;
void refresh_vision(Individual * individual);
void generate_map();

static inline bool is_in_bounds(Coord point) {
    return point.y >= 0 && point.y < map_size.y && point.x >= 0 && point.x < map_size.x;
}

#endif
