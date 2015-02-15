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

struct WandInfo {
    WandDescriptionId description_id;
    int charges;
};

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

enum ThingType {
    ThingType_WAND,
    ThingType_INDIVIDUAL,
};

class ThingImpl : public ReferenceCounted {
public:
    uint256 id;
    ThingType thing_type;
    // this is set to false in the time between actually being destroyed and being removed from the master list
    bool still_exists = true;

    Coord location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;

    StatusEffects status_effects;

    // individual
    ThingImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker);

    // wand
    ThingImpl(WandDescriptionId description_id, int charges);


    ThingImpl(ThingImpl &) = delete;
    ~ThingImpl();

    Life * life() {
        if (thing_type != ThingType_INDIVIDUAL)
            panic("wrong type");
        return _life;
    }
    WandInfo * wand_info() {
        if (thing_type != ThingType_WAND)
            panic("wrong type");
        return _wand_info;
    }
private:
    union {
        Life * _life;
        WandInfo * _wand_info;
    };
};
typedef Reference<ThingImpl> Thing;

class FilteredIterator {
public:
    bool next(Thing * output) {
        while (iterator.next(output))
            if (filter(*output))
                return true;
        return false;
    }
    FilteredIterator(const IdMap<Thing> & hashtable, bool (*filter)(Thing)) :
            iterator(hashtable.value_iterator()), filter(filter) {
    }
private:
    IdMap<Thing>::Iterator iterator;
    bool (*filter)(Thing);
};

PerceivedThing to_perceived_individual(uint256 target_id);
PerceivedThing observe_individual(Thing observer, Thing target);

int compare_individuals_by_initiative(Thing a, Thing b);

// TODO: these are in the wrong place
void compute_vision(Thing individual);
void get_item_description(Thing observer, uint256 wielder_id, uint256 item_id, ByteBuffer * output);
void zap_wand(Thing individual, uint256 item_id, Coord direction);

#endif
