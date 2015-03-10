#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "reference_counter.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "hashtable.hpp"
#include "list.hpp"
#include "byte_buffer.hpp"
#include "string.hpp"
#include "text.hpp"

#include <stdbool.h>

enum ThingType {
    ThingType_WAND,
    ThingType_INDIVIDUAL,
};

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
    SpeciesId_ANT,
    SpeciesId_BEE,
    SpeciesId_BEETLE,
    SpeciesId_SCORPION,
    SpeciesId_SNAKE,

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
    int base_hitpoints;
    int base_attack_power;
    VisionTypes vision_types;
    bool has_mind;
    bool sucks_up_items;
    bool auto_throws_items;
    bool uses_wands;
};

struct StatusEffects {
    bool invisible = false;
    int confused_timeout = 0;
};

struct PerceivedWandInfo {
    WandDescriptionId description_id;
};
struct PerceivedLife {
    SpeciesId species_id;
    Team team;
};

class PerceivedThingImpl : public ReferenceCounted {
public:
    uint256 id;
    ThingType thing_type;
    Coord location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;
    StatusEffects status_effects;
    // individual
    PerceivedThingImpl(uint256 id, SpeciesId species_id, Coord location, Team team, StatusEffects status_effects) :
            id(id), thing_type(ThingType_INDIVIDUAL), location(location), status_effects(status_effects) {
        life() = {
            species_id,
            team,
        };
    }
    // item
    PerceivedThingImpl(uint256 id, WandDescriptionId description_id, Coord location, StatusEffects status_effects) :
            id(id), thing_type(ThingType_WAND), location(location), status_effects(status_effects) {
        wand_info() = {
            description_id,
        };
    }
    PerceivedThingImpl(uint256 id, WandDescriptionId description_id, uint256 container_id, int z_order, StatusEffects status_effects) :
            id(id), thing_type(ThingType_WAND), container_id(container_id), z_order(z_order), status_effects(status_effects) {
        wand_info() = {
            description_id,
        };
    }
    PerceivedLife & life() {
        if (thing_type != ThingType_INDIVIDUAL)
            panic("wrong type");
        return _life;
    }
    PerceivedWandInfo & wand_info() {
        if (thing_type != ThingType_WAND)
            panic("wrong type");
        return _wand_info;
    }
private:
    union {
        PerceivedLife _life;
        PerceivedWandInfo _wand_info;
    };
};
typedef Reference<PerceivedThingImpl> PerceivedThing;

struct RememberedEventImpl : public ReferenceCounted {
    Span span = new_span();
};
typedef Reference<RememberedEventImpl> RememberedEvent;

struct Knowledge {
    // terrain knowledge
    MapMatrix<Tile> tiles;
    MapMatrix<VisionTypes> tile_is_visible;
    List<RememberedEvent> remembered_events;
    // incremented whenever events were forgotten, which means we need to blank out and refresh the rendering of the events.
    int event_forget_counter = 0;
    // this is never wrong
    WandId wand_identities[WandId_COUNT];
    IdMap<PerceivedThing> perceived_things;
    Knowledge() {
        reset_map();
        for (int i = 0; i < WandId_COUNT; i++)
            wand_identities[i] = WandId_UNKNOWN;
    }
    void reset_map() {
        tiles.set_all(unknown_tile);
        tile_is_visible.set_all(VisionTypes::none());
    }
};

static inline int experience_to_level(int experience) {
    return log2(experience);
}
// the lowest experience for this level
static inline int level_to_experience(int level) {
    return 1 << level;
}

struct Life {
    SpeciesId species_id;
    int hitpoints;
    long long hp_regen_deadline;
    int experience = 0;
    long long movement_ready_time = 0;
    uint256 initiative;
    Team team;
    DecisionMakerType decision_maker;
    Knowledge knowledge;

    Species * species() const;
    int experience_level() const {
        return experience_to_level(experience);
    }
    int next_level_up() const {
        return level_to_experience(experience_level() + 1);
    }
    int attack_power() const {
        return max(1, (species()->base_attack_power * (experience_level() + 1)) / 2);
    }
    int max_hitpoints() const {
        return species()->base_hitpoints + 2 * experience_level();
    }
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

    ThingImpl(ThingImpl &) = delete;
    ThingImpl & operator=(ThingImpl &) = delete;
};
typedef Reference<ThingImpl> Thing;


template<typename T>
static inline bool is_individual(T thing) {
    return thing->thing_type == ThingType_INDIVIDUAL;
}
template<typename T>
static inline bool is_item(T thing) {
    return thing->thing_type == ThingType_WAND;
}

static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_individuals(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_individual);
}
static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_items(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_item);
}

PerceivedThing to_perceived_thing(uint256 target_id);
PerceivedThing perceive_thing(Thing observer, Thing target);

static inline int compare_individuals_by_initiative(Thing a, Thing b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
static inline int compare_things_by_id(Thing a, Thing b) {
    return compare(a->id, b->id);
}

// TODO: these are in the wrong place
void compute_vision(Thing observer);
void zap_wand(Thing individual, uint256 item_id, Coord direction);

#endif
