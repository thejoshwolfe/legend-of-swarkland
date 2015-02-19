#include "map.hpp"

#include "individual.hpp"
#include "swarkland.hpp"

MapMatrix<Tile> actual_map_tiles;

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

static void refresh_normal_vision(Thing individual) {
    Coord you_location = individual->location;
    for (Coord target = {0, 0}; target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            if (!is_open_line_of_sight(you_location, target))
                continue;
            individual->life()->knowledge.tile_is_visible[target].normal = true;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

static const int ethereal_radius = 5;
static void refresh_ethereal_vision(Thing individual) {
    Coord you_location = individual->location;
    Coord etheral_radius_diagonal = {ethereal_radius, ethereal_radius};
    Coord upper_left = clamp(you_location - etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    Coord lower_right= clamp(you_location + etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    for (Coord target = upper_left; target.y <= lower_right.y; target.y++) {
        for (target.x = upper_left.x; target.x <= lower_right.x; target.x++) {
            if (euclidean_distance_squared(target, you_location) > ethereal_radius * ethereal_radius)
                continue;
            individual->life()->knowledge.tile_is_visible[target].ethereal = true;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

void compute_vision(Thing observer) {
    // mindless monsters can't remember the terrain
    if (!observer->life()->species()->has_mind)
        observer->life()->knowledge.tiles.set_all(unknown_tile);

    observer->life()->knowledge.tile_is_visible.set_all(VisionTypes::none());
    if (observer->life()->species()->vision_types.normal)
        refresh_normal_vision(observer);
    if (observer->life()->species()->vision_types.ethereal)
        refresh_ethereal_vision(observer);

    // see individuals
    // first clear out anything that we know are no longer where we thought
    List<PerceivedThing> remove_these;
    PerceivedThing target;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        Coord target_location = get_thing_location(observer, target);
        if (target_location == Coord::nowhere())
            continue;
        if (observer->life()->species()->has_mind && !observer->life()->knowledge.tile_is_visible[target_location].any())
            continue;
        remove_these.append(target);
        List<PerceivedThing> inventory;
        find_items_in_inventory(observer, target, &inventory);
        remove_these.append_all(inventory);
    }
    // do this as a second pass, because modifying in the middle of iteration doesn't work properly.
    for (int i = 0; i < remove_these.length(); i++)
        observer->life()->knowledge.perceived_things.remove(remove_these[i]->id);

    // now see anything that's in our line of vision
    Thing actual_target;
    for (auto iterator = actual_things.value_iterator(); iterator.next(&actual_target);) {
        if (!actual_target->still_exists)
            continue;
        PerceivedThing perceived_target = perceive_thing(observer, actual_target);
        if (perceived_target == NULL)
            continue;
        observer->life()->knowledge.perceived_things.put(perceived_target->id, perceived_target);
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
    int rock_count = random_int(20, 40);
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
