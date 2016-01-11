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
    ThingType_INDIVIDUAL,
    ThingType_WAND,
    ThingType_POTION,
};

enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,
    WandDescriptionId_COPPER_WAND,
    WandDescriptionId_PURPLE_WAND,

    WandDescriptionId_COUNT,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_STRIKING,
    WandId_WAND_OF_SPEED,
    WandId_WAND_OF_REMEDY,

    WandId_COUNT,
    WandId_UNKNOWN,
};

enum PotionDescriptionId {
    PotionDescriptionId_BLUE_POTION,
    PotionDescriptionId_GREEN_POTION,
    PotionDescriptionId_RED_POTION,
    PotionDescriptionId_YELLOW_POTION,
    PotionDescriptionId_ORANGE_POTION,
    PotionDescriptionId_PURPLE_POTION,

    PotionDescriptionId_COUNT,
};
enum PotionId {
    PotionId_POTION_OF_HEALING,
    PotionId_POTION_OF_POISON,
    PotionId_POTION_OF_ETHEREAL_VISION,
    PotionId_POTION_OF_COGNISCOPY,
    PotionId_POTION_OF_BLINDNESS,
    PotionId_POTION_OF_INVISIBILITY,

    PotionId_COUNT,
    PotionId_UNKNOWN,
};

extern WandId actual_wand_identities[WandDescriptionId_COUNT];
extern PotionId actual_potion_identities[PotionDescriptionId_COUNT];

enum SpeciesId {
    SpeciesId_HUMAN,
    SpeciesId_OGRE,
    SpeciesId_LICH,
    SpeciesId_PINK_BLOB,
    SpeciesId_AIR_ELEMENTAL,
    SpeciesId_DOG,
    SpeciesId_ANT,
    SpeciesId_BEE,
    SpeciesId_BEETLE,
    SpeciesId_SCORPION,
    SpeciesId_SNAKE,

    SpeciesId_COUNT,
    SpeciesId_UNSEEN,
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

// everyone has the same action cost
static const int action_cost = 12;

struct Species {
    // how many ticks does it cost to move one space? average human is 12.
    int movement_cost;
    int base_hitpoints;
    int base_attack_power;
    // this range is only for random spawning
    int min_level;
    int max_level;
    VisionTypes vision_types;
    bool has_mind;
    bool sucks_up_items;
    bool auto_throws_items;
    bool poison_attack;
    bool uses_wands;
    bool uses_potions;
    bool advanced_strategy;
};

struct StatusEffect {
    enum StatusEffectType {
        CONFUSION,
        SPEED,
        ETHEREAL_VISION,
        COGNISCOPY,
        BLINDNESS,
        INVISIBILITY,
        POISON,
    };
    StatusEffectType type;
    // this is never in the past
    int64_t expiration_time;

    // currently only used for poison
    int64_t poison_next_damage_time;
    // used for awarding experience for poison damage kills
    uint256 who_is_responsible;
};

struct PerceivedWandInfo {
    WandDescriptionId description_id;
};
struct PerceivedPotionInfo {
    PotionDescriptionId description_id;
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
    int64_t last_seen_time;
    List<StatusEffect::StatusEffectType> status_effects;
    // individual
    PerceivedThingImpl(uint256 id, SpeciesId species_id, Coord location, Team team, int64_t last_seen_time) :
            id(id), thing_type(ThingType_INDIVIDUAL), location(location), last_seen_time(last_seen_time) {
        assert(id != uint256::zero());
        if (id == uint256{16834101961345315201ULL, 6672530469245184391ULL, 8551116969905452987ULL, 4392448728760151208ULL}) {
            id = true ? id : id;
        }
        _life = create<PerceivedLife>();
        _life->species_id = species_id;
        _life->team = team;
    }
    // wand
    PerceivedThingImpl(uint256 id, WandDescriptionId description_id, Coord location, uint256 container_id, int z_order, int64_t last_seen_time) :
            id(id), thing_type(ThingType_WAND), location(location), container_id(container_id), z_order(z_order), last_seen_time(last_seen_time) {
        _wand_info = create<PerceivedWandInfo>();
        _wand_info->description_id = description_id;
    }
    // potion
    PerceivedThingImpl(uint256 id, PotionDescriptionId description_id, Coord location, uint256 container_id, int z_order, int64_t last_seen_time) :
            id(id), thing_type(ThingType_POTION), location(location), container_id(container_id), z_order(z_order), last_seen_time(last_seen_time) {
        _potion_info = create<PerceivedPotionInfo>();
        _potion_info->description_id = description_id;
    }
    ~PerceivedThingImpl() {
        switch (thing_type) {
            case ThingType_INDIVIDUAL:
                destroy(_life, 1);
                break;
            case ThingType_WAND:
                destroy(_wand_info, 1);
                break;
            case ThingType_POTION:
                destroy(_potion_info, 1);
                break;
        }
    }
    PerceivedLife * life() {
        if (thing_type != ThingType_INDIVIDUAL)
            panic("wrong type");
        return _life;
    }
    PerceivedWandInfo * wand_info() {
        if (thing_type != ThingType_WAND)
            panic("wrong type");
        return _wand_info;
    }
    PerceivedPotionInfo * potion_info() {
        if (thing_type != ThingType_POTION)
            panic("wrong type");
        return _potion_info;
    }
private:
    union {
        PerceivedLife * _life;
        PerceivedWandInfo * _wand_info;
        PerceivedPotionInfo * _potion_info;
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

    // these are never wrong
    WandId wand_identities[WandDescriptionId_COUNT];
    PotionId potion_identities[PotionDescriptionId_COUNT];

    IdMap<PerceivedThing> perceived_things;
    Knowledge() {
        reset_map();
        for (int i = 0; i < WandDescriptionId_COUNT; i++)
            wand_identities[i] = WandId_UNKNOWN;
        for (int i = 0; i < PotionDescriptionId_COUNT; i++)
            potion_identities[i] = PotionId_UNKNOWN;
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

struct Life : public PerceivedLife {
    int hitpoints;
    int64_t hp_regen_deadline;
    int experience = 0;
    int64_t last_movement_time = 0;
    int64_t last_action_time = 0;
    uint256 initiative;
    DecisionMakerType decision_maker;
    Knowledge knowledge;

    const Species * species() const;
    int experience_level() const {
        return experience_to_level(experience);
    }
    int next_level_up() const {
        return level_to_experience(experience_level() + 1);
    }
    int attack_power() const {
        return max(1, species()->base_attack_power + experience_level() / 2);
    }
    int max_hitpoints() const {
        return species()->base_hitpoints + 2 * experience_level();
    }
};

struct WandInfo : public PerceivedWandInfo {
    int charges;
};
struct PotionInfo : public PerceivedPotionInfo {
    // it's the same
};

class ThingImpl : public ReferenceCounted {
public:
    uint256 id;
    const ThingType thing_type;
    // this is set to false in the time between actually being destroyed and being removed from the master list
    bool still_exists = true;

    Coord location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;

    List<StatusEffect> status_effects;

    // individual
    ThingImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker);
    // wand
    ThingImpl(WandDescriptionId description_id, int charges);
    // potion
    ThingImpl(PotionDescriptionId description_id);

    ~ThingImpl() {
        switch (thing_type) {
            case ThingType_INDIVIDUAL:
                destroy(_life, 1);
                break;
            case ThingType_WAND:
                destroy(_wand_info, 1);
                break;
            case ThingType_POTION:
                destroy(_potion_info, 1);
                break;
        }
    }

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
    PotionInfo * potion_info() {
        if (thing_type != ThingType_POTION)
            panic("wrong type");
        return _potion_info;
    }
private:
    union {
        Life * _life;
        WandInfo * _wand_info;
        PotionInfo * _potion_info;
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
    return thing->thing_type != ThingType_INDIVIDUAL;
}

static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_individuals(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_individual);
}
static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_items(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_item);
}

PerceivedThing to_perceived_thing(uint256 target_id);

static inline int compare_individuals_by_initiative(Thing a, Thing b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
static inline int compare_things_by_id(Thing a, Thing b) {
    return compare(a->id, b->id);
}

// TODO: this is in the wrong place
void compute_vision(Thing observer);

static inline int find_status(const List<StatusEffect> & status_effects, StatusEffect::StatusEffectType status) {
    for (int i = 0; i < status_effects.length(); i++)
        if (status_effects[i].type == status)
            return i;
    return -1;
}
static inline StatusEffect * find_or_put_status(Thing thing, StatusEffect::StatusEffectType status) {
    int index = find_status(thing->status_effects, status);
    if (index == -1) {
        index = thing->status_effects.length();
        thing->status_effects.append(StatusEffect { status, -1, -1, uint256::zero() });
    }
    return &thing->status_effects[index];
}

static inline bool has_status(const List<StatusEffect> & status_effects, StatusEffect::StatusEffectType status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status(Thing thing, StatusEffect::StatusEffectType status) {
    return has_status(thing->status_effects, status);
}
static inline int find_status(const List<StatusEffect::StatusEffectType> & status_effects, StatusEffect::StatusEffectType status) {
    for (int i = 0; i < status_effects.length(); i++)
        if (status_effects[i] == status)
            return i;
    return -1;
}
static inline bool has_status(const List<StatusEffect::StatusEffectType> & status_effects, StatusEffect::StatusEffectType status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status(PerceivedThing thing, StatusEffect::StatusEffectType status) {
    return has_status(thing->status_effects, status);
}
static inline void put_status(PerceivedThing thing, StatusEffect::StatusEffectType status) {
    if (!has_status(thing, status))
        thing->status_effects.append(status);
}
static inline void maybe_remove_status(PerceivedThing thing, StatusEffect::StatusEffectType status) {
    int index = find_status(thing->status_effects, status);
    if (index != -1)
        thing->status_effects.swap_remove(index);
}

#endif
