#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "reference_counter.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "hashtable.hpp"

#include <stdbool.h>

enum SpeciesId {
    SpeciesId_HUMAN,
    SpeciesId_OGRE,
    SpeciesId_DOG,
    SpeciesId_GELATINOUS_CUBE,
    SpeciesId_DUST_VORTEX,

    SpeciesId_COUNT,
};

enum AiStrategy {
    // this is you
    AiStrategy_PLAYER,
    // if you can see the player, move and attack
    AiStrategy_ATTACK_IF_VISIBLE,
};

struct VisionTypes {
    unsigned normal : 1;
    unsigned ethereal : 1;

    bool any() const {
        return normal || ethereal;
    }
};

static const VisionTypes no_vision = {0, 0};

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

struct PerceivedIndividualImpl : public ReferenceCounted {
public:
    uint256 id;
    bool is_alive;
    Species * species;
    Coord location;
    bool invisible;
    PerceivedIndividualImpl(uint256 id, bool is_alive, Species * species, Coord location, bool invisible) :
            id(id), is_alive(is_alive), species(species), location(location), invisible(invisible) {
    }
};
typedef Reference<PerceivedIndividualImpl> PerceivedIndividual;

class Knowledge {
public:
    // terrain knowledge
    Coord map_last_observed_from = {-1, -1};
    Matrix<Tile> tiles;
    Matrix<VisionTypes> tile_is_visible;

    IdMap<PerceivedIndividual> perceived_individuals;
    Knowledge() :
            tiles(map_size), tile_is_visible(map_size) {
        tiles.set_all(unknown_tile);
        tile_is_visible.set_all(no_vision);
    }
};

struct IndividualImpl : public ReferenceCounted {
    uint256 id;
    bool is_alive = true;
    Species * species;
    int hitpoints;
    int kill_counter = 0;
    Coord location;
    // once this reaches movement_cost, make a move
    int movement_points = 0;
    AiStrategy ai;
    Knowledge knowledge;
    bool invisible = false;
    IndividualImpl(SpeciesId species_id, Coord location);
    IndividualImpl(IndividualImpl &) = delete;
};
typedef Reference<IndividualImpl> Individual;

PerceivedIndividual to_perceived_individual(Individual target);
PerceivedIndividual observe_individual(Individual observer, Individual target);

// TODO: this is in the wrong place
void compute_vision(Individual individual);

#endif
