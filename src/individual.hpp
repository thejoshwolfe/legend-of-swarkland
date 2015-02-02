#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "geometry.hpp"
#include "map.hpp"
#include "vision_types.hpp"

#include <stdbool.h>

enum SpeciesId {
    SpeciesId_HUMAN, //
    SpeciesId_OGRE, //
    SpeciesId_DOG, //
    SpeciesId_GELATINOUS_CUBE, //
    SpeciesId_DUST_VORTEX, //

    SpeciesId_COUNT, //
};

enum AiStrategy {
    // this is you
    AiStrategy_PLAYER,
    // if you can see the player, move and attack
    AiStrategy_ATTACK_IF_VISIBLE,
};

struct Species {
    SpeciesId species_id;
    // how many ticks does it cost to move one space? average human is 12.
    int movement_cost;
    int starting_hitpoints;
    int attack_power;
    AiStrategy default_ai;
    VisionTypes vision_types;
    bool has_mind;
};

struct Individual {
    bool is_alive = true;
    Species * species;
    int hitpoints;
    int kill_counter = 0;
    Coord location;
    // once this reaches movement_cost, make a move
    int movement_points = 0;
    AiStrategy ai;
    Coord bumble_destination = Coord(-1, -1);
    Knowledge knowledge;
    long long vision_last_calculated_time = -1;
    bool invisible = false;
    Individual(SpeciesId species_id, Coord location);
    Individual(Individual &) = delete;
};

#endif
