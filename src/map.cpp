#include "map.hpp"

#include "individual.hpp"
#include "swarkland.hpp"

Matrix<Tile> actual_map_tiles(map_size.y, map_size.x);

static bool is_open_line_of_sight(Coord from_location, Coord to_location) {
    if (from_location == to_location)
        return true;
    Coord abs_delta(abs(to_location.x - from_location.x), abs(to_location.y - from_location.y));
    if (abs_delta.y > abs_delta.x) {
        // primarily vertical
        int dy = sign(to_location.y - from_location.y);
        for (int y = from_location.y + dy; y * dy < to_location.y * dy; y += dy) {
            // x = y * m + b
            // m = run / rise
            int x = (y - from_location.y) * (to_location.x - from_location.x) / (to_location.y - from_location.y) + from_location.x;
            if (actual_map_tiles[Coord(x, y)].tile_type == TileType_WALL)
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (actual_map_tiles[Coord(x, y)].tile_type == TileType_WALL)
                return false;
        }
    }
    return true;
}

void refresh_vision(Individual * individual) {
    if (individual->vision_last_calculated_time == time_counter)
        return; // he already knows
    individual->vision_last_calculated_time = time_counter;
    Coord you_location = individual->location;
    for (Coord target(0, 0); target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            bool visible = is_open_line_of_sight(you_location, target);
            individual->believed_map.is_visible[target] = visible;
            if (visible)
                individual->believed_map.tiles[target] = actual_map_tiles[target];
        }
    }
}

