#include "swarkland.hpp"

Species * specieses[SpeciesId_COUNT];
List<Individual *> individuals;
Individual * you;
long long time_counter = 0;

void init_specieses() {
    specieses[SpeciesId_HUMAN] = new Species(SpeciesId_HUMAN, 12, 10, 3);
    specieses[SpeciesId_OGRE] = new Species(SpeciesId_OGRE, 24, 10, 2);
    specieses[SpeciesId_DOG] = new Species(SpeciesId_DOG, 12, 4, 2);
    specieses[SpeciesId_GELATINOUS_CUBE] = new Species(SpeciesId_GELATINOUS_CUBE, 48, 12, 4);
    specieses[SpeciesId_DUST_VORTEX] = new Species(SpeciesId_DUST_VORTEX, 6, 8, 1);
}

static void init_individuals() {
    individuals.add(new Individual(SpeciesId_HUMAN, Coord(4, 4), AiStrategy_PLAYER));
    you = individuals.at(0);
    individuals.add(new Individual(SpeciesId_OGRE, Coord(10, 10), AiStrategy_LEROY_JENKINS));
    individuals.add(new Individual(SpeciesId_OGRE, Coord(20, 15), AiStrategy_LEROY_JENKINS));
    individuals.add(new Individual(SpeciesId_DOG, Coord(5, 20), AiStrategy_LEROY_JENKINS));
    individuals.add(new Individual(SpeciesId_GELATINOUS_CUBE, Coord(0, 0), AiStrategy_BUMBLE_AROUND));
    individuals.add(new Individual(SpeciesId_DUST_VORTEX, Coord(55, 29), AiStrategy_BUMBLE_AROUND));
}

void swarkland_init() {
    init_specieses();

    init_individuals();
}
static void attack(Individual * attacker, Individual * target) {
    target->hitpoints -= attacker->species->attack_power;
    if (target->hitpoints <= 0) {
        target->hitpoints = 0;
        target->is_alive = false;
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

void move_with_ai(Individual * individual) {
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

