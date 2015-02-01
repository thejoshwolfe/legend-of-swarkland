#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "geometry.hpp"

#include <stdbool.h>

enum SpeciesId {
    SpeciesId_HUMAN, //
    SpeciesId_OGRE, //

    SpeciesId_COUNT, //
};

enum AiStrategy {
    // this is you
    AiStrategy_PLAYER,
    // always move toward the player and attack without hesitation
    AiStrategy_LEROY_JENKINS,
};

struct Species {
    SpeciesId species_id;
    // how many ticks does it cost to move one space? average human is 12.
    int movement_cost;
    int starting_hitpoints;
    int attack_power;
    Species(SpeciesId species_id, int movement_cost, int starting_hitpoints, int attack_power) :
            species_id(species_id), movement_cost(movement_cost), starting_hitpoints(starting_hitpoints), attack_power(attack_power) {
    }
    Species(Species &) = delete;
};

struct Individual {
    bool is_alive;
    Species * species;
    int hitpoints;
    Coord location;
    AiStrategy ai;
    Individual(SpeciesId species_id, Coord location, AiStrategy ai);
    Individual(Individual &) = delete;
};

#endif
