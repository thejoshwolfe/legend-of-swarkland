#include "swarkland.hpp"

#include "path_finding.hpp"

Species * specieses[SpeciesId_COUNT];
List<Individual *> individuals;
Individual * you;
long long time_counter = 0;

bool cheatcode_full_visibility;

static void init_specieses() {
    specieses[SpeciesId_HUMAN] = new Species(SpeciesId_HUMAN, 12, 10, 3, AiStrategy_ATTACK_IF_VISIBLE);
    specieses[SpeciesId_OGRE] = new Species(SpeciesId_OGRE, 24, 10, 2, AiStrategy_ATTACK_IF_VISIBLE);
    specieses[SpeciesId_DOG] = new Species(SpeciesId_DOG, 12, 4, 2, AiStrategy_ATTACK_IF_VISIBLE);
    specieses[SpeciesId_GELATINOUS_CUBE] = new Species(SpeciesId_GELATINOUS_CUBE, 48, 12, 4, AiStrategy_ATTACK_IF_VISIBLE);
    specieses[SpeciesId_DUST_VORTEX] = new Species(SpeciesId_DUST_VORTEX, 6, 6, 1, AiStrategy_ATTACK_IF_VISIBLE);
}

static Individual * spawn_a_monster(SpeciesId species_id) {
    while (species_id == SpeciesId_COUNT) {
        species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // humans are too hard. without giving one side a powerup, they're evenly matched.
            species_id = SpeciesId_COUNT;
        }
    }
    Coord location;
    while (true) {
        location = Coord(random_int(map_size.x), random_int(map_size.y));
        if (find_individual_at(location) != NULL)
            continue;
        if (actual_map_tiles[location].tile_type == TileType_WALL)
            continue;
        break;
    }
    Individual * individual = new Individual(species_id, location);
    individuals.add(individual);
    return individual;
}

static void init_individuals() {
    you = spawn_a_monster(SpeciesId_HUMAN);
    you->ai = AiStrategy_PLAYER;

    // generate a few warm-up monsters
    for (int i = 0; i < 6; i++) {
        spawn_a_monster(SpeciesId_COUNT);
    }
}

static void generate_map() {
    // randomize the appearance of every tile, even if it doesn't matter.
    for (Coord cursor(0, 0); cursor.y < map_size.y; cursor.y++) {
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

void swarkland_init() {
    init_specieses();

    generate_map();

    init_individuals();
}
void swarkland_finish() {
    // this is to make valgrind happy. valgrind is useful when it's happy.
    for (int i = 0; i < individuals.size(); i++)
        delete individuals.at(i);
    for (int i = 0; i < SpeciesId_COUNT; i++)
        delete specieses[i];
}

static void spawn_monsters() {
    if (random_int(120) == 0)
        spawn_a_monster(SpeciesId_COUNT);
}
static void heal_the_living() {
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (!individual->is_alive)
            continue;
        if (individual->hitpoints < individual->species->starting_hitpoints) {
            if (random_int(60) == 0) {
                individual->hitpoints++;
            }
        }
    }
}

static void attack(Individual * attacker, Individual * target) {
    target->hitpoints -= attacker->species->attack_power;
    if (target->hitpoints <= 0) {
        target->hitpoints = 0;
        target->is_alive = false;
        attacker->kill_counter++;
    }
}

static void move_toward_point(Individual * individual, Coord point) {
    List<Coord> path;
    find_path(individual->location, point, individual->believed_map.tiles, path);
    if (path.size() > 0) {
        Coord new_position = path.at(0);
        if (new_position.x == you->location.x && new_position.y == you->location.y) {
            attack(individual, you);
        } else {
            individual->location = new_position;
        }
    } else {
        // TODO: now we're stuck forever
    }
}

static void move_leroy_jenkins(Individual * individual) {
    move_toward_point(individual, you->location);
}

static void move_bumble_around(Individual * individual) {
    if (individual->believed_map.is_visible[you->location]) {
        // there he is!
        move_leroy_jenkins(individual);
        // if we lose him. reroll our new destination.
        individual->bumble_destination = Coord(-1, -1);
    } else {
        // idk where 2 go
        if (individual->bumble_destination == Coord(-1, -1))
            individual->bumble_destination = individual->location;
        while (individual->bumble_destination == individual->location) {
            // choose a random place to go.
            // don't look too far, or else the monsters end up sweeping back and forth in straight lines. it looks dumb.
            int min_x = clamp(individual->location.x - 5, 0, map_size.x - 1);
            int max_x = clamp(individual->location.x + 5, 0, map_size.x - 1);
            int min_y = clamp(individual->location.y - 5, 0, map_size.y - 1);
            int max_y = clamp(individual->location.y + 5, 0, map_size.y - 1);
            individual->bumble_destination = Coord(random_int(min_x, max_x + 1), random_int(min_y, max_y + 1));
            if (individual->believed_map.tiles[individual->bumble_destination].tile_type == TileType_WALL) {
                // try again
                // TODO: this can infinite loop
                individual->bumble_destination = individual->location;
            }
        }
        move_toward_point(individual, individual->bumble_destination);
    }
}

Individual * find_individual_at(Coord location) {
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (!individual->is_alive)
            continue;
        if (individual->location.x == location.x && individual->location.y == location.y)
            return individual;
    }
    return NULL;
}

static void move_with_ai(Individual * individual) {
    refresh_vision(individual);
    if (individual->ai != AiStrategy_ATTACK_IF_VISIBLE)
        panic("unknown ai strategy");
    move_bumble_around(individual);
}
bool take_action(bool just_wait, Coord delta) {
    refresh_vision(you);
    if (cheatcode_spectator != NULL) {
        // doing this early shouldn't affect anything
        refresh_vision(cheatcode_spectator);
    }

    if (!you->is_alive)
        return false;
    if (just_wait)
        return true;
    if (delta.x == 0 && delta.y == 0)
        return false; // not moving
    Coord new_position(you->location.x + delta.x, you->location.y + delta.y);
    if (new_position.x < 0 || new_position.x >= map_size.x || new_position.y < 0 || new_position.y >= map_size.y)
        return false;
    if (you->believed_map.tiles[new_position].tile_type == TileType_WALL)
        return false;
    Individual * target = find_individual_at(new_position);
    if (target != NULL && target != you) {
        attack(you, target);
    } else {
        you->location = new_position;
    }
    return true;
}

void advance_time() {
    time_counter++;

    spawn_monsters();
    heal_the_living();

    // award movement points to the living
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (!individual->is_alive)
            continue;
        individual->movement_points++;
    }

    // move monsters
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (!individual->is_alive)
            continue;
        if (individual->ai == AiStrategy_PLAYER)
            continue;
        if (individual->movement_points >= individual->species->movement_cost) {
            // make a move
            individual->movement_points = 0;
            move_with_ai(individual);
        }
    }
}

void cheatcode_kill_everybody_in_the_world() {
    for (int i = 0; i < individuals.size(); i++)
        individuals.at(i)->is_alive = false;
    // you're cool
    you->is_alive = true;
}
void cheatcode_polymorph() {
    SpeciesId species_id = you->species->species_id;
    species_id = (SpeciesId)((species_id + 1) % SpeciesId_COUNT);
    you->species = specieses[species_id];
}
Individual * cheatcode_spectator = NULL;
void cheatcode_spectate() {
    for (int i = 0; i < individuals.size(); i++) {
        Individual * individual = individuals.at(i);
        if (!individual->is_alive)
            continue;
        if (individual == you)
            continue;
        if (individual == cheatcode_spectator) {
            cheatcode_spectator = NULL;
        } else if (cheatcode_spectator == NULL) {
            cheatcode_spectator = individual;
            break;
        }
    }
}
