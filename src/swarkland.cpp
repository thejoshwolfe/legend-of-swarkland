#include "swarkland.hpp"

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

static void move_leroy_jenkins(Individual * individual) {
    int dx = sign(you->location.x - individual->location.x);
    int dy = sign(you->location.y - individual->location.y);
    Coord new_position = { clamp(individual->location.x + dx, 0, map_size.x - 1), clamp(individual->location.y + dy, 0, map_size.y - 1), };
    if (new_position.x == you->location.x && new_position.y == you->location.y) {
        attack(individual, you);
    } else {
        individual->location = new_position;
    }
}

static void move_bumble_around(Individual * individual) {
    // TODO
    move_leroy_jenkins(individual);
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
    Individual * badguy = find_individual_at(new_position);
    if (badguy != NULL) {
        attack(you, badguy);
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
