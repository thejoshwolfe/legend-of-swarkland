#include "swarkland.hpp"

#include <stdlib.h>

Species * specieses[SpeciesId_COUNT];
List<Individual *> individuals;
Individual * you;
long long time_counter = 0;

void init_specieses() {
    specieses[SpeciesId_HUMAN] = new Species(SpeciesId_HUMAN, 12, 10, 3, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_OGRE] = new Species(SpeciesId_OGRE, 24, 10, 2, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_DOG] = new Species(SpeciesId_DOG, 12, 4, 2, AiStrategy_LEROY_JENKINS);
    specieses[SpeciesId_GELATINOUS_CUBE] = new Species(SpeciesId_GELATINOUS_CUBE, 48, 12, 4, AiStrategy_BUMBLE_AROUND);
    specieses[SpeciesId_DUST_VORTEX] = new Species(SpeciesId_DUST_VORTEX, 6, 6, 1, AiStrategy_BUMBLE_AROUND);
}

static void init_individuals() {
    individuals.add(new Individual(SpeciesId_HUMAN, Coord(4, 4)));
    you = individuals.at(0);
    you->ai = AiStrategy_PLAYER;

    // here's a few warm-up monsters
    individuals.add(new Individual(SpeciesId_OGRE, Coord(10, 10)));
    individuals.add(new Individual(SpeciesId_OGRE, Coord(20, 15)));
    individuals.add(new Individual(SpeciesId_DOG, Coord(5, 20)));
    individuals.add(new Individual(SpeciesId_GELATINOUS_CUBE, Coord(0, 0)));
    individuals.add(new Individual(SpeciesId_DUST_VORTEX, Coord(55, 29)));
}

void swarkland_init() {
    init_specieses();
    init_individuals();
}

static void spawn_monsters() {
    if (random_int(120) == 0) {
        SpeciesId species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // human's are too hard. without giving one side a powerup, they're evenly matched.
            return;
        }
        Coord location(random_int(map_size.x), random_int(map_size.y));
        individuals.add(new Individual(species_id, location));
    }
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
    int dx = sign(point.x - individual->location.x);
    int dy = sign(point.y - individual->location.y);
    Coord new_position = { clamp(individual->location.x + dx, 0, map_size.x - 1), clamp(individual->location.y + dy, 0, map_size.y - 1), };
    if (new_position.x == you->location.x && new_position.y == you->location.y) {
        attack(individual, you);
    } else {
        individual->location = new_position;
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
        }
        move_toward_point(individual, individual->bumble_destination);
    }
}

static Individual * find_individual_at(Coord location) {
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
void you_move(Coord delta) {
    if (!you->is_alive)
        return;
    Coord new_position(clamp(you->location.x + delta.x, 0, map_size.x - 1), clamp(you->location.y + delta.y, 0, map_size.y - 1));
    Individual * target = find_individual_at(new_position);
    if (target != NULL && target != you) {
        attack(you, target);
    } else {
        you->location = new_position;
    }
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
