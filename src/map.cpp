#include "map.hpp"

#include "thing.hpp"
#include "swarkland.hpp"
#include "item.hpp"
#include "event.hpp"

bool is_open_line_of_sight(Coord from_location, Coord to_location, const MapMatrix<TileType> & map_tiles) {
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
            if (!is_open_space(map_tiles[Coord{x, y}]))
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (!is_open_space(map_tiles[Coord{x, y}]))
                return false;
        }
    }
    return true;
}

static void see_map_with_normal_vision(Thing individual) {
    Coord you_location = individual->location;
    for (Coord target = {0, 0}; target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            if (!is_open_line_of_sight(you_location, target, game->actual_map_tiles))
                continue;
            individual->life()->knowledge.tile_is_visible[target] |= VisionTypes_NORMAL;
            individual->life()->knowledge.tiles[target] = game->actual_map_tiles[target];
            individual->life()->knowledge.aesthetic_indexes[target] = game->aesthetic_indexes[target];
        }
    }
}

static void see_map_with_ethereal_vision(Thing individual) {
    Coord you_location = individual->location;
    Coord etheral_radius_diagonal = {ethereal_radius, ethereal_radius};
    Coord upper_left = clamp(you_location - etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    Coord lower_right= clamp(you_location + etheral_radius_diagonal, Coord{0, 0}, map_size - Coord{1, 1});
    for (Coord target = upper_left; target.y <= lower_right.y; target.y++) {
        for (target.x = upper_left.x; target.x <= lower_right.x; target.x++) {
            if (euclidean_distance_squared(target, you_location) > ethereal_radius * ethereal_radius)
                continue;
            individual->life()->knowledge.tile_is_visible[target] |= VisionTypes_ETHEREAL;
            individual->life()->knowledge.tiles[target] = game->actual_map_tiles[target];
            individual->life()->knowledge.aesthetic_indexes[target] = game->aesthetic_indexes[target];
        }
    }
}

void record_shape_of_terrain(MapMatrix<TileType> * tiles, Coord location) {
    if (game->actual_map_tiles[location] == TileType_STAIRS_DOWN) {
        (*tiles)[location] = TileType_STAIRS_DOWN;
    } else {
        bool is_air = is_open_space(game->actual_map_tiles[location]);
        if ((*tiles)[location] == TileType_UNKNOWN || is_open_space((*tiles)[location]) != is_air) {
            (*tiles)[location] = is_air ? TileType_UNKNOWN_FLOOR : TileType_UNKNOWN_WALL;
        }
    }
}

void see_aesthetics(Thing observer) {
    Knowledge & knowledge = observer->life()->knowledge;
    for (Coord target = {0, 0}; target.y < map_size.y; target.y++)
        for (target.x = 0; target.x < map_size.x; target.x++)
            if (can_see_color(knowledge.tile_is_visible[target]))
                knowledge.aesthetic_indexes[target] = game->aesthetic_indexes[target];
}

void compute_vision(Thing observer) {
    Knowledge & knowledge = observer->life()->knowledge;

    VisionTypes no_vision_yet = 0;
    if (has_status(observer, StatusEffect::COGNISCOPY)) {
        // cogniscopy reaches everywhere
        no_vision_yet |= VisionTypes_COGNISCOPY;
    }
    // refresh vision of the map
    knowledge.tile_is_visible.set_all(no_vision_yet);
    VisionTypes has_vision = observer->physical_species()->vision_types;
    if (has_status(observer, StatusEffect::ETHEREAL_VISION)) {
        has_vision &= ~VisionTypes_NORMAL;
        has_vision |= VisionTypes_ETHEREAL;
    } else if (has_status(observer, StatusEffect::BLINDNESS)) {
        has_vision &= ~VisionTypes_NORMAL;
    }
    if (has_vision & VisionTypes_NORMAL)
        see_map_with_normal_vision(observer);
    if (has_vision & VisionTypes_ETHEREAL)
        see_map_with_ethereal_vision(observer);
    // you can always feel just the spot you're on
    knowledge.tile_is_visible[observer->location] |= VisionTypes_COLOCATION;
    record_shape_of_terrain(&knowledge.tiles, observer->location);
    // while blind (and always), you're reaching around you and feel if the walls around you are real or not
    for (Coord cursor = {-1, -1}; cursor.y <= 1; cursor.y++) {
        for (cursor.x = -1; cursor.x <= 1; cursor.x++) {
            knowledge.tile_is_visible[observer->location + cursor] |= VisionTypes_REACH_AND_TOUCH;
            record_shape_of_terrain(&knowledge.tiles, observer->location + cursor);
        }
    }

    // see things
    // first clear out anything that we know is no longer where we thought
    PerceivedThing target;
    for (auto iterator = knowledge.perceived_things.value_iterator(); iterator.next(&target);) {
        Coord target_location = get_top_level_container(observer, target)->location;
        if (target_location == Coord::nowhere())
            continue;
        VisionTypes vision = knowledge.tile_is_visible[target_location];
        // cogniscopy cannot verify the absence of things at a location
        vision &= ~VisionTypes_COGNISCOPY;
        // reach and touch doesn't feel things
        vision &= ~VisionTypes_REACH_AND_TOUCH;

        if (vision == 0)
            continue; // leave the marker
        if (is_invisible(observer, target) && !can_see_physical_presence(vision)) {
            // we had reason to believe there was something invisible here, and we don't have confidence that it's gone.
            // leave the marker.
            continue;
        }
        target->location = Coord::nowhere();
    }

    // now see anything that's in our line of vision
    Thing actual_target;
    for (auto iterator = game->actual_things.value_iterator(); iterator.next(&actual_target);) {
        if (!actual_target->still_exists)
            continue;
        if (can_see_thing(observer, actual_target->id))
            record_perception_of_thing(observer, actual_target->id);
    }
}

static inline uint8_t random_aesthetic_index() {
    // only go from 0-f so that serializing it can be a single character.
    return (uint8_t)random_uint32() & 0x0f;
}

void animate_map_tiles() {
    for (Coord cursor = { 0, 0 }; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            switch (game->actual_map_tiles[cursor]) {
                case TileType_LAVA_FLOOR:
                    // actually animated
                    if (random_int(24, nullptr) == 0) {
                        // switch on average every 2 turns.
                        game->aesthetic_indexes[cursor] = (game->aesthetic_indexes[cursor] + 1) & 0x0f;
                    }
                    break;

                case TileType_DIRT_FLOOR:
                case TileType_MARBLE_FLOOR:
                case TileType_UNKNOWN_FLOOR:
                case TileType_BROWN_BRICK_WALL:
                case TileType_GRAY_BRICK_WALL:
                case TileType_BORDER_WALL:
                case TileType_UNKNOWN_WALL:
                case TileType_STAIRS_DOWN:
                    break;

                case TileType_UNKNOWN:
                case TileType_COUNT:
                    unreachable();
            }
        }
    }
}

void generate_map() {
    game->dungeon_level++;

    // randomize the appearance of every tile, even though it doesn't matter.
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            game->actual_map_tiles[cursor] = TileType_BROWN_BRICK_WALL;
            if (!game->test_mode)
                game->aesthetic_indexes[cursor] = random_aesthetic_index();
            else
                game->aesthetic_indexes[cursor] = 0;
        }
    }
    // line the border with special undiggable walls
    for (int x = 0; x < map_size.x; x++) {
        game->actual_map_tiles[Coord{x, 0}] = TileType_BORDER_WALL;
        game->actual_map_tiles[Coord{x, map_size.y - 1}] = TileType_BORDER_WALL;
    }
    for (int y = 0; y < map_size.y; y++) {
        game->actual_map_tiles[Coord{0, y}] = TileType_BORDER_WALL;
        game->actual_map_tiles[Coord{map_size.x - 1, y}] = TileType_BORDER_WALL;
    }

    if (game->test_mode) {
        // basic test map
        // a room
        for (int y = 1; y < 5; y++)
            for (int x = 1; x < 5; x++)
                game->actual_map_tiles[Coord{x, y}] = TileType_DIRT_FLOOR;
        // a hallway, so there's a "just around the corner"
        for (int x = 5; x < 10; x++)
            game->actual_map_tiles[Coord{x, 4}] = TileType_DIRT_FLOOR;
        // no stairs
        return;
    }

    // create rooms
    List<SDL_Rect> rooms;
    List<Coord> room_floor_spaces;
    for (int i = 0; i < 50; i++) {
        int width = random_int(3, 10, nullptr);
        int height = random_int(3, 10, nullptr);
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
    for (int i = 0; i < rooms.length(); i++) {
        SDL_Rect room = rooms[i];
        bool room_contains_lava = game->dungeon_level >= 4 && room.w >= 5 && room.h >= 5 && random_int(3, nullptr) == 0;
        Coord cursor;
        for (cursor.y = room.y + 1; cursor.y < room.y + room.h - 1; cursor.y++) {
            for (cursor.x = room.x + 1; cursor.x < room.x + room.w - 1; cursor.x++) {
                if (room_contains_lava && random_int(3, nullptr) == 0) {
                    // lava
                    game->actual_map_tiles[cursor] = TileType_LAVA_FLOOR;
                } else {
                    room_floor_spaces.append(cursor);
                    game->actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
                }
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
            game->actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
        }
        for (; cursor.y * delta.y < b.y * delta.y; cursor.y += delta.y) {
            game->actual_map_tiles[cursor] = TileType_DIRT_FLOOR;
        }
    }

    // place the stairs down
    if (game->dungeon_level < final_dungeon_level) {
        Coord stairs_down_location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        game->actual_map_tiles[stairs_down_location] = TileType_STAIRS_DOWN;
    }

    // throw some items around
    if (game->dungeon_level == 1) {
        // first level always has a wand of digging and a potion of ethereal vision to make finding vaults less random.
        Coord location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        create_wand(WandId_WAND_OF_DIGGING)->location = location;
        location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        create_potion(PotionId_POTION_OF_ETHEREAL_VISION)->location = location;
    }
    int item_count = random_inclusive(3, 6, nullptr);
    for (int i = 0; i < item_count; i++) {
        Coord location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
        create_random_item()->location = location;
    }

    // place some vaults
    int vault_count = random_inclusive(1, 2, nullptr);
    int width = 6;
    int height = 6;
    for (int i = 0; i < vault_count; i++) {
        List<Coord> available_locations;
        Coord position;
        for (position.y = 1; position.y < map_size.y - 1; position.y++) {
            for (position.x = 1; position.x < map_size.x - 1; position.x++) {
                Coord cursor;
                for (cursor.y = position.y; cursor.y < position.y + height; cursor.y++)
                    for (cursor.x = position.x; cursor.x < position.x + width; cursor.x++)
                        if (game->actual_map_tiles[cursor] != TileType_BROWN_BRICK_WALL)
                            goto next_position;
                // this location is secluded
                available_locations.append(position);
                next_position:;
            }
        }
        if (available_locations.length() > 0) {
            position = available_locations[random_int(available_locations.length(), nullptr)];
            SDL_Rect room = SDL_Rect{position.x, position.y, width, height};
            // line the border with a different color wall
            for (int x = room.x + 1; x < room.x + room.w - 1; x++) {
                game->actual_map_tiles[Coord{x, room.y + 1}] = TileType_GRAY_BRICK_WALL;
                game->actual_map_tiles[Coord{x, room.y + room.h - 2}] = TileType_GRAY_BRICK_WALL;
            }
            for (int y = room.y + 1; y < room.y + room.h - 1; y++) {
                game->actual_map_tiles[Coord{room.x + 1, y}] = TileType_GRAY_BRICK_WALL;
                game->actual_map_tiles[Coord{room.x + room.w - 2, y}] = TileType_GRAY_BRICK_WALL;
            }
            Coord cursor;
            for (cursor.y = room.y + 2; cursor.y < room.y + room.h - 2; cursor.y++) {
                for (cursor.x = room.x + 2; cursor.x < room.x + room.w - 2; cursor.x++) {
                    game->actual_map_tiles[cursor] = TileType_MARBLE_FLOOR;
                    create_random_item()->location = cursor;
                }
            }
        } else {
            // throw what items would be in the vault around in the rooms
            for (int i = 0; i < 4; i++) {
                Coord location = room_floor_spaces[random_int(room_floor_spaces.length(), nullptr)];
                create_random_item()->location = location;
            }
        }
    }
}

static const int no_spawn_radius = 10;
static bool can_spawn_at(Coord away_from_location, Coord location) {
    TileType tile = game->actual_map_tiles[location];
    if (!is_safe_space(tile))
        return false;
    if (tile == TileType_MARBLE_FLOOR)
        return false; // don't spawn in vaults
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

