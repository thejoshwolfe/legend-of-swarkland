#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "reference_counter.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "hashtable.hpp"
#include "list.hpp"
#include "byte_buffer.hpp"

#include <stdbool.h>


enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,

    WandDescriptionId_COUNT,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_STRIKING,

    WandId_COUNT,
    WandId_UNKNOWN,
};

extern WandId actual_wand_identities[WandId_COUNT];

struct ItemImpl : public ReferenceCounted {
    uint256 id;
    WandDescriptionId description_id;
    Coord floor_location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;
    int charges;
    ItemImpl(uint256 id, WandDescriptionId description_id, int charges) :
            id(id), description_id(description_id), charges(charges) {
    }
};
typedef Reference<ItemImpl> Item;


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

struct StatusEffects {
    bool invisible = false;
    int confused_timeout = 0;
};

struct PerceivedIndividualImpl : public ReferenceCounted {
public:
    uint256 id;
    SpeciesId species_id;
    Coord location;
    Team team;
    StatusEffects status_effects;
    PerceivedIndividualImpl(uint256 id, SpeciesId species_id, Coord location, Team team, StatusEffects status_effects) :
            id(id), species_id(species_id), location(location), team(team), status_effects(status_effects) {
    }
};
typedef Reference<PerceivedIndividualImpl> PerceivedThing;

struct RememberedEventImpl : public ReferenceCounted {
    ByteBuffer bytes;
};
typedef Reference<RememberedEventImpl> RememberedEvent;

struct Knowledge {
    // terrain knowledge
    MapMatrix<Tile> tiles;
    MapMatrix<VisionTypes> tile_is_visible;
    List<RememberedEvent> remembered_events;
    // this is never wrong
    WandId wand_identities[WandId_COUNT];
    IdMap<PerceivedThing> perceived_individuals;
    Knowledge() {
        tiles.set_all(unknown_tile);
        tile_is_visible.set_all(VisionTypes::none());
        for (int i = 0; i < WandId_COUNT; i++)
            wand_identities[i] = WandId_UNKNOWN;
    }
};

struct Life {
    SpeciesId species_id;
    int hitpoints;
    int kill_counter = 0;
    // once this reaches movement_cost, make a move
    int movement_points = 0;
    uint256 initiative;
    Team team;
    DecisionMakerType decision_maker;
    Knowledge knowledge;

    Species * species() const;
};

class ThingImpl : public ReferenceCounted {
public:
    uint256 id;
    // this is set to false in the time between actually being destroyed and being removed from the master list
    bool still_exists = true;
    Coord location;
    StatusEffects status_effects;
    ThingImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker);
    ThingImpl(ThingImpl &) = delete;
    ~ThingImpl();

    Life * life() {
        return _life;
    }
private:
    Life * _life;
};
typedef Reference<ThingImpl> Thing;


PerceivedThing to_perceived_individual(uint256 target_id);
PerceivedThing observe_individual(Thing observer, Thing target);

int compare_individuals_by_initiative(Thing a, Thing b);

// TODO: these are in the wrong place
void compute_vision(Thing individual);
void get_item_description(Thing observer, uint256 wielder_id, uint256 item_id, ByteBuffer * output);
void zap_wand(Thing individual, uint256 item_id, Coord direction);

#endif
