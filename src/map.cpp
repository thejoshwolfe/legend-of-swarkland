#include "map.hpp"

#include "thing.hpp"
#include "swarkland.hpp"
#include "item.hpp"
#include "event.hpp"

// starts at 1
int dungeon_level = 0;
MapMatrix<TileType> actual_map_tiles;
MapMatrix<uint32_t> aesthetic_indexes;
MapMatrix<bool> spawn_zone;
Coord stairs_down_location;

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
            if (!is_open_space(actual_map_tiles[Coord{x, y}]))
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (!is_open_space(actual_map_tiles[Coord{x, y}]))
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
            individual->life()->knowledge.tile_is_visible[target] |= VisionTypes_NORMAL;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

const int ethereal_radius = 5;
static void refresh_ethereal_vision(Thing individual) {
    Coord you_location = individual->location;
    Coord etheral_radius_diagonal = {ethereal_radius, ethereal_radius};
    Coord upper_left = clamp(you_location - etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    Coord lower_right= clamp(you_location + etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    for (Coord target = upper_left; target.y <= lower_right.y; target.y++) {
        for (target.x = upper_left.x; target.x <= lower_right.x; target.x++) {
            if (euclidean_distance_squared(target, you_location) > ethereal_radius * ethereal_radius)
                continue;
            individual->life()->knowledge.tile_is_visible[target] |= VisionTypes_ETHEREAL;
            individual->life()->knowledge.tiles[target] = actual_map_tiles[target];
        }
    }
}

void compute_vision(Thing observer) {
    Knowledge & knowledge = observer->life()->knowledge;

    VisionTypes no_vision_yet = 0;
    if (has_status(observer, StatusEffect::COGNISCOPY)) {
        // cogniscopy reaches everywhere
        no_vision_yet |= VisionTypes_COGNISCOPY;
    }
    knowledge.tile_is_visible.set_all(no_vision_yet);
    VisionTypes has_vision = observer->life()->species()->vision_types;
    if (has_status(observer, StatusEffect::ETHEREAL_VISION)) {
        has_vision &= ~VisionTypes_NORMAL;
        has_vision |= VisionTypes_ETHEREAL;
    } else if (has_status(observer, StatusEffect::BLINDNESS)) {
        has_vision &= ~VisionTypes_NORMAL;
    }
    if (has_vision & VisionTypes_NORMAL)
        refresh_normal_vision(observer);
    if (has_vision & VisionTypes_ETHEREAL)
        refresh_ethereal_vision(observer);
    // you can always feel just the spot you're on
    knowledge.tile_is_visible[observer->location] |= VisionTypes_TOUCH;

    // see things
    // first clear out anything that we know is no longer where we thought
    PerceivedThing target;
    for (auto iterator = knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        Coord target_location = get_thing_location(observer, target);
        if (target_location == Coord::nowhere())
            continue;
        VisionTypes vision = knowledge.tile_is_visible[target_location];
        // TODO: allow cogniscopy to clear some markers
        vision &= ~VisionTypes_COGNISCOPY;

        if (vision == 0)
            continue; // leave the marker
        if (is_invisible(observer, target) && !can_see_invisible(vision)) {
            // we had reason to believe there was something invisible here, and we don't have confidence that it's gone.
            // leave the marker.
            continue;
        }
        target->location = Coord::nowhere();
    }

    // now see anything that's in our line of vision
    Thing actual_target;
    for (auto iterator = actual_things.value_iterator(); iterator.next(&actual_target);) {
        if (!actual_target->still_exists)
            continue;
        if (can_see_thing(observer, actual_target->id))
            record_perception_of_thing(observer, actual_target->id);
    }
}

void generate_map() {
    dungeon_level++;

    spawn_zone.set_all(true);
    // randomize the appearance of every tile, even if it doesn't matter.
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            actual_map_tiles[cursor] = TileType_WALL;
            if (!test_mode)
                aesthetic_indexes[cursor] = random_uint32();
            else
                aesthetic_indexes[cursor] = (cursor.x / 5 + cursor.y / 5);
        }
    }
    // line the border with special undiggable walls
    for (int x = 0; x < map_size.x; x++) {
        actual_map_tiles[Coord{x, 0}] = TileType_BORDER_WALL;
        actual_map_tiles[Coord{x, map_size.y - 1}] = TileType_BORDER_WALL;
    }
    for (int y = 0; y < map_size.y; y++) {
        actual_map_tiles[Coord{0, y}] = TileType_BORDER_WALL;
        actual_map_tiles[Coord{map_size.x - 1, y}] = TileType_BORDER_WALL;
    }

    if (test_mode) {
        // basic test map
        // a room
        for (int y = 1; y < 5; y++)
            for (int x = 1; x < 5; x++)
                actual_map_tiles[Coord{x, y}] = TileType_DIRT_FLOOR;
        // a hallway, so there's a "just around the corner"
        for (int x = 5; x < 10; x++)
            actual_map_tiles[Coord{x, 4}] = TileType_DIRT_FLOOR;
        // no stairs
        stairs_down_location = Coord::nowhere();
        return;
    }

    // create rooms
    List<SDL_Rect> rooms;
    for (int i = 0; i < 50; i++) {
        int width = random_int(4, 10, nullptr);
        int height = random_int(4, 10, nullptr);
        int x = random_int(0, map_size.x - width, nullptr);
        int y = random_int(0, map_size.y - height, nullptr);
        SDL_Rect room = SDL_Rect{x, y, width, height};
        for (int j = 0; j < rooms.length(); j++) {
            SDL_Rect intersection;
            if (SDL_IntersectRect(&rooms[j], &room, &intersection)) {
                // overlaps with another room.
                // don't create this room at all.
                goto next_room;
            }
        }
        rooms.append(room);
        next_room:;
    }
    List<Coord> room_floor_spaces;
    for (int i = 0; i < rooms.length(); i++) {
        SDL_Rect room = rooms[i];
        Coord cursor;
        for (cursor.y = room.y + 1; cursor.y < room.y + room.h - 1; cursor.y++) {
            for (cursor.x = room.x + 1; cursor.x < room.x + room.w - 1; cursor.x++) {
                room_floor_spaces.append(cursor);
                actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
            }
        }
    }

    // connect rooms with prim's algorithm, or something.
    struct PrimUtil {
        static inline int find_root_node(const List<int> & node_to_parent_node, int node) {
            while (true) {
                int parent_node = node_to_parent_node[node];
                if (parent_node == node)
                    return node;
                node = parent_node;
            }
        }
        static inline void merge(List<int> * node_to_parent_node, int a, int b) {
            (*node_to_parent_node)[a] = b;
        }
    };
    List<int> node_to_parent_node;
    for (int i = 0; i < rooms.length(); i++)
        node_to_parent_node.append(i);
    for (int i = 0; i < rooms.length(); i++) {
        SDL_Rect room = rooms[i];
        int room_root = PrimUtil::find_root_node(node_to_parent_node, i);
        // find nearest room
        // uh... this is not prim's algorithm. we're supposed to find the shortest edge in the graph.
        // whatever.
        int closest_neighbor = -1;
        SDL_Rect closest_room = {-1, -1, -1, -1};
        int closest_neighbor_root = -1;
        int closest_distance = 0x7fffffff;
        for (int j = 0; j < rooms.length(); j++) {
            SDL_Rect other_room = rooms[j];
            int other_root = PrimUtil::find_root_node(node_to_parent_node, j);
            if (room_root == other_root)
                continue; // already joined
            int distance = ordinal_distance(Coord{room.x, room.y}, Coord{other_room.x, other_room.y});
            if (distance < closest_distance) {
                closest_neighbor = j;
                closest_room = other_room;
                closest_neighbor_root = other_root;
                closest_distance = distance;
            }
        }
        if (closest_neighbor == -1)
            continue; // already connected to everyone
        // connect to neighbor
        PrimUtil::merge(&node_to_parent_node, room_root, closest_neighbor_root);
        // derpizontal, and then derpicle
        Coord a = {room.x + 1, room.y + 1};
        Coord b = {closest_room.x + 1, closest_room.y + 1};
        Coord delta = sign(b - a);
        Coord cursor = a;
        for (; cursor.x * delta.x < b.x * delta.x; cursor.x += delta.x) {
            actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
        }
        for (; cursor.y * delta.y < b.y * delta.y; cursor.y += delta.y) {
            actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
        }
    }

    // place the stairs down
    if (dungeon_level < final_dungeon_level) {
        stairs_down_location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        actual_map_tiles[stairs_down_location] = TileType_STAIRS_DOWN;
    }

    // throw some items around
    int wand_count = random_inclusive(1, 2, nullptr);
    for (int i = 0; i < wand_count; i++) {
        Coord location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        Thing item;
        if (dungeon_level == 1 && i == 0) {
            // first level always has a wand of digging to make finding vaults less random.
            create_wand(WandId_WAND_OF_DIGGING)->location = location;
        } else {
            create_random_item(ThingType_WAND)->location = location;
        }
    }
    int potion_count = random_inclusive(2, 4, nullptr);
    for (int i = 0; i < potion_count; i++) {
        Coord location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        if (dungeon_level == 1 && i == 0) {
            // first level always has a potion of ethereal vision to make finding vaults less random.
            create_potion(PotionId_POTION_OF_ETHEREAL_VISION)->location = location;
        } else {
            create_random_item(ThingType_POTION)->location = location;
        }
    }

    // place some vaults
    int vault_count = random_inclusive(1, 2, nullptr);
    for (int i = 0; i < 10; i++) {
        int width = 4;
        int height = 4;
        int x = random_int(0, map_size.x - width, nullptr);
        int y = random_int(0, map_size.y - height, nullptr);
        SDL_Rect room = SDL_Rect{x, y, width, height};

        Coord cursor;
        for (cursor.y = room.y; cursor.y < room.y + room.h; cursor.y++)
            for (cursor.x = room.x; cursor.x < room.x + room.w; cursor.x++)
                if (actual_map_tiles[cursor] != TileType_WALL)
                    goto next_vault;
        // this location is secluded

        for (cursor.y = room.y + 1; cursor.y < room.y + room.h - 1; cursor.y++) {
            for (cursor.x = room.x + 1; cursor.x < room.x + room.w - 1; cursor.x++) {
                actual_map_tiles[cursor] = TileType_MARBLE_FLOOR;
                spawn_zone[cursor] = false;
                create_random_item()->location = cursor;
            }
        }

        vault_count--;
        if (vault_count == 0)
            break;

        next_vault:;
    }
}

static const int no_spawn_radius = 10;
bool can_spawn_at(Coord away_from_location, Coord location) {
    if (!is_open_space(actual_map_tiles[location]))
        return false;
    if (!spawn_zone[location])
        return false;
    if (away_from_location != Coord::nowhere() && euclidean_distance_squared(location, away_from_location) < no_spawn_radius * no_spawn_radius)
        return false;
    if (find_individual_at(location) != nullptr)
        return false;
    return true;
}

Coord random_spawn_location(Coord away_from_location) {
    List<Coord> available_spawn_locations;
    for (Coord location = {0, 0}; location.y < map_size.y; location.y++)
        for (location.x = 0; location.x < map_size.x; location.x++)
            if (can_spawn_at(away_from_location, location))
                available_spawn_locations.append(location);
    if (available_spawn_locations.length() == 0)
        return Coord::nowhere();
    return available_spawn_locations[random_int(available_spawn_locations.length(), nullptr)];
}

Coord find_stairs_down_location() {
    for (Coord location = {0, 0}; location.y < map_size.y; location.y++)
        for (location.x = 0; location.x < map_size.x; location.x++)
            if (actual_map_tiles[location] == TileType_STAIRS_DOWN)
                return location;
    unreachable();
}
