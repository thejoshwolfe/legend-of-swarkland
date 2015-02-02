#include "swarkland.hpp"

#include "path_finding.hpp"

#include <stdlib.h>

Map the_map;

Species * specieses[SpeciesId_COUNT];
List<Individual *> individuals;
Individual * you;
long long time_counter = 0;

static void init_specieses() {
    specieses[SpeciesId_HUMAN] = new Species(SpeciesId_HUMAN, 12, 10, 3, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_OGRE] = new Species(SpeciesId_OGRE, 24, 10, 2, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_DOG] = new Species(SpeciesId_DOG, 12, 4, 2, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_GELATINOUS_CUBE] = new Species(SpeciesId_GELATINOUS_CUBE, 48, 12, 4, AiStrategy_BUMBLE_AROUND);
    specieses[SpeciesId_DUST_VORTEX] = new Species(SpeciesId_DUST_VORTEX, 6, 6, 1, AiStrategy_BUMBLE_AROUND);
}

static Individual * spawn_a_monster(SpeciesId species_id) {
    while (species_id == SpeciesId_COUNT) {
        species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // human's are too hard. without giving one side a powerup, they're evenly matched.
            species_id = SpeciesId_COUNT;
        }
    }
    Coord location;
    while (true) {
        location = Coord(random_int(map_size.x), random_int(map_size.y));
        if (find_individual_at(location) != NULL)
            continue;
        if (!the_map.tiles[location].is_open)
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
            Tile & tile = the_map.tiles[cursor];
            tile.is_open = true;
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
                the_map.tiles[cursor].is_open = false;
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
            if (!the_map.tiles[Coord(x, y)].is_open)
                return false;
        }
    } else {
        // primarily horizontal
        int dx = sign(to_location.x - from_location.x);
        for (int x = from_location.x + dx; x * dx < to_location.x * dx; x += dx) {
            // y = x * m + b
            // m = rise / run
            int y = (x - from_location.x) * (to_location.y - from_location.y) / (to_location.x - from_location.x) + from_location.y;
            if (!the_map.tiles[Coord(x, y)].is_open)
                return false;
        }
    }
    return true;
}

static long long vision_last_calculated_time = -1;
void refresh_vision() {
    if (vision_last_calculated_time == time_counter)
        return; // we already know
    vision_last_calculated_time = time_counter;
    Coord you_location = you->location;
    for (Coord target(0, 0); target.y < map_size.y; target.y++) {
        for (target.x = 0; target.x < map_size.x; target.x++) {
            the_map.tiles[target].is_visible = is_open_line_of_sight(you_location, target);
        }
    }
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
    if (find_path(individual->location, point, path)) {
        Coord new_position = path.at(0);
        if (new_position.x == you->location.x && new_position.y == you->location.y) {
            attack(individual, you);
        } else {
            individual->location = new_position;
        }
    } else {
        // no clera path.
        // TODO: do our best anyway
    }
}

static void move_leroy_jenkins(Individual * individual) {
    move_toward_point(individual, you->location);
}

static void move_bumble_around(Individual * individual) {
    int dx = you->location.x - individual->location.x;
    int dy = you->location.y - individual->location.y;
    int distance = abs(dx) + abs(dy);
    if (distance < 10) {
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
            if (!the_map.tiles[individual->bumble_destination].is_open) {
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
    switch (individual->ai) {
        case AiStrategy_LEROY_JENKINS:
            move_leroy_jenkins(individual);
            break;
        case AiStrategy_BUMBLE_AROUND:
            move_bumble_around(individual);
            break;
        case AiStrategy_PLAYER:
            panic("player is not ai");
            break;
    }
}
bool you_move(Coord delta) {
    refresh_vision();

    if (!you->is_alive)
        return false;
    if (delta.x == 0 && delta.y == 0)
        return false; // not moving
    Coord new_position(you->location.x + delta.x, you->location.y + delta.y);
    if (new_position.x < 0 || new_position.x >= map_size.x || new_position.y < 0 || new_position.y >= map_size.y)
        return false;
    if (!the_map.tiles[new_position].is_open)
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
