#ifndef MAP_HPP
#define MAP_HPP

#include "geometry.hpp"

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

class Map {
public:
    Matrix<Tile> tiles;
    Matrix<bool> is_visible;
    Map() :
            tiles(map_size.y, map_size.x), is_visible(map_size.y, map_size.x) {
        tiles.set_all(Tile());
        is_visible.set_all(false);
    }
};

extern Matrix<Tile> actual_map_tiles;

struct Individual;
void refresh_vision(Individual * individual);

#endif