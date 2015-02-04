#include "map.hpp"

#include "individual.hpp"
#include "swarkland.hpp"

Matrix<Tile> actual_map_tiles(map_size);

static bool is_open_line_of_sight(Coord from_location, Coord to_location) {
    if (from_location == to_location)
        return true;
    Coord abs_delta = {abs(to_location.x - from_location.x), abs(to_location.y - from_location.y)};
    if (abs_delta.y > abs_delta.x) {
        // primarily vertical
        int dy = sign(to_location.y - from_location.y);
        for (int y = from_location.y + dy; y * dy < to_location.y * dy; y += dy) {
            // x = y * m + b
            // m = run / rise
            int x = (y - from_location.y) * (to_location.x - from_location.x) / (to_location.y - from_location.y) + from_location.x;
            if (actual_map_tiles[Coord{x, y}].tile_type == TileType_WALL)
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (actual_map_tiles[{x, y}].tile_type == TileType_WALL)
                return false;
        }
    }
    return true;
}

static void refresh_normal_vision(Individual individual) {
    Coord you_location = individual->location;
    for (Coord target = {0, 0}; target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            if (!is_open_line_of_sight(you_location, target))
                continue;
            individual->knowledge.tile_is_visible[target].normal = true;
            individual->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

static const int ethereal_radius = 5;
static void refresh_ethereal_vision(Individual individual) {
    Coord you_location = individual->location;
    Coord upper_left = clamp(Coord{you_location.x - ethereal_radius, you_location.y - ethereal_radius}, Coord{0, 0}, map_size);
    Coord lower_right= clamp(Coord{you_location.x + ethereal_radius + 1, you_location.y + ethereal_radius + 1}, Coord{0, 0}, map_size);
    for (Coord target = upper_left; target.y < lower_right.y; target.y++) {
        for (target.x = 0; target.x < lower_right.x; target.x++) {
            if (distance_squared(target, you_location) > ethereal_radius * ethereal_radius)
                continue;
            individual->knowledge.tile_is_visible[target].ethereal = true;
            individual->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

void refresh_vision(Individual spectator) {
    if (spectator->knowledge.map_last_observed_from != spectator->location) {
        // take a look at the terrain
        spectator->knowledge.map_last_observed_from = spectator->location;

        // mindless monsters can't remember the terrain
        if (!spectator->species->has_mind)
            spectator->knowledge.tiles.set_all(unknown_tile);
        spectator->knowledge.tile_is_visible.set_all(no_vision);

        if (spectator->species->vision_types.normal)
            refresh_normal_vision(spectator);
        if (spectator->species->vision_types.ethereal)
            refresh_ethereal_vision(spectator);
    }

    // see individuals
    // first clear out any monsters that we know are no longer where we thought
    for (auto iterator = spectator->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual target = iterator.next();
        if (!spectator->species->has_mind || spectator->knowledge.tile_is_visible[target->location].any())
            spectator->knowledge.perceived_individuals.remove(target->id);
    }
    // now see any monsters that are in our line of vision
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual target = observe_individual(spectator, iterator.next());
        if (target == NULL)
            continue;
        spectator->knowledge.perceived_individuals.put_or_overwrite(target->id, target);
    }
}

void generate_map() {
    // randomize the appearance of every tile, even if it doesn't matter.
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            Tile & tile = actual_map_tiles[cursor];
            tile.tile_type = TileType_FLOOR;
            tile.aesthetic_index = random_int(8);
        }
    }
    // generate some obstructions.
    // they're all rectangles for now
    int rock_count = random_int(20, 50);
    for (int i = 0; i < rock_count; i++) {
        int width = random_int(2, 8);
        int height = random_int(2, 8);
        int x = random_int(0, map_size.x - width);
        int y = random_int(0, map_size.y - height);
        Coord cursor;
        for (cursor.y = y; cursor.y < y + height; cursor.y++) {
            for (cursor.x = x; cursor.x < x + width; cursor.x++) {
                actual_map_tiles[cursor].tile_type = TileType_WALL;
            }
        }
    }
}
