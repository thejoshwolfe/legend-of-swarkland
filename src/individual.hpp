#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "reference_counter.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "hashtable.hpp"
#include "list.hpp"
#include "byte_buffer.hpp"

#include <stdbool.h>

enum SpeciesId {
    SpeciesId_HUMAN,
    SpeciesId_OGRE,
    SpeciesId_DOG,
    SpeciesId_PINK_BLOB,
    SpeciesId_AIR_ELEMENTAL,

    SpeciesId_COUNT,
};

enum Team {
    Team_GOOD_GUYS,
    Team_BAD_GUYS,
};

enum DecisionMakerType {
    DecisionMakerType_PLAYER,
    DecisionMakerType_AI,

    DecisionMakerType_COUNT,
};

struct VisionTypes {
    unsigned normal : 1;
    unsigned ethereal : 1;

    bool any() const {
        return normal || ethereal;
    }
    static inline VisionTypes none() {
        return {0, 0};
    }
};
static inline bool operator==(VisionTypes a, VisionTypes b) {
    return a.normal == b.normal && a.ethereal == b.ethereal;
}
static inline bool operator!=(VisionTypes a, VisionTypes b) {
    return !(a == b);
}

struct Species {
    SpeciesId species_id;
    // how many ticks does it cost to move one space? average human is 12.
    int movement_cost;
    int starting_hitpoints;
    int attack_power;
    VisionTypes vision_types;
    bool has_mind;
};

struct PerceivedIndividualImpl : public ReferenceCounted {
public:
    uint256 id;
    SpeciesId species_id;
    Coord location;
    Team team;
    bool invisible;
    PerceivedIndividualImpl(uint256 id, SpeciesId species_id, Coord location, Team team, bool invisible) :
            id(id), species_id(species_id), location(location), team(team), invisible(invisible) {
    }
};
typedef Reference<PerceivedIndividualImpl> PerceivedIndividual;

struct RememberedEventImpl : public ReferenceCounted {
    ByteBuffer bytes;
};
typedef Reference<RememberedEventImpl> RememberedEvent;

class Knowledge {
public:
    // terrain knowledge
    Coord map_last_observed_from = Coord::nowhere();
    VisionTypes map_last_observed_with;
    Matrix<Tile> tiles;
    Matrix<VisionTypes> tile_is_visible;
    List<RememberedEvent> remembered_events;

    IdMap<PerceivedIndividual> perceived_individuals;
    Knowledge() :
            tiles(map_size), tile_is_visible(map_size) {
        tiles.set_all(unknown_tile);
        tile_is_visible.set_all(VisionTypes::none());
    }
};

struct IndividualImpl : public ReferenceCounted {
    uint256 id;
    SpeciesId species_id;
    bool is_alive = true;
    int hitpoints;
    int kill_counter = 0;
    Coord location;
    // once this reaches movement_cost, make a move
    int movement_points = 0;
    uint256 initiative;
    Team team;
    DecisionMakerType decision_maker;
    Knowledge knowledge;
    bool invisible = false;
    IndividualImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker);
    IndividualImpl(IndividualImpl &) = delete;
    Species * species() const;
};
typedef Reference<IndividualImpl> Individual;

PerceivedIndividual to_perceived_individual(Individual target);
PerceivedIndividual observe_individual(Individual observer, Individual target);

// TODO: this is in the wrong place
void compute_vision(Individual individual);

int compare_individuals_by_initiative(Individual a, Individual b);

#endif
