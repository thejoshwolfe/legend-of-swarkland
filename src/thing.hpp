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

uint256 random_id();

enum ThingType {
    ThingType_INDIVIDUAL,
    ThingType_WAND,
    ThingType_POTION,
    ThingType_BOOK,

    ThingType_COUNT,
};

enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,
    WandDescriptionId_COPPER_WAND,
    WandDescriptionId_PURPLE_WAND,

    WandDescriptionId_COUNT,
    WandDescriptionId_UNSEEN,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_MAGIC_MISSILE,
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
    PotionDescriptionId_UNSEEN,
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

enum BookDescriptionId {
    BookDescriptionId_PURPLE_BOOK,
    BookDescriptionId_BLUE_BOOK,
    BookDescriptionId_RED_BOOK,
    BookDescriptionId_GREEN_BOOK,

    BookDescriptionId_COUNT,
    BookDescriptionId_UNSEEN,
};
enum BookId {
    BookId_SPELLBOOK_OF_MAGIC_BULLET,
    BookId_SPELLBOOK_OF_SPEED,
    BookId_SPELLBOOK_OF_MAPPING,
    BookId_SPELLBOOK_OF_FORCE,

    BookId_COUNT,
    BookId_UNKNOWN,
};

extern WandDescriptionId actual_wand_descriptions[WandId_COUNT];
extern PotionDescriptionId actual_potion_descriptions[PotionId_COUNT];
extern BookDescriptionId actual_book_descriptions[BookId_COUNT];

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
    SpeciesId_COBRA,

    SpeciesId_COUNT,
    SpeciesId_UNSEEN,
};

enum DecisionMakerType {
    DecisionMakerType_AI,
    DecisionMakerType_PLAYER,

    DecisionMakerType_COUNT,
};

typedef uint8_t VisionTypes;
enum VisionTypesBits {
    VisionTypes_NORMAL = 0x1,
    VisionTypes_ETHEREAL = 0x2,
    VisionTypes_COGNISCOPY = 0x4,
    VisionTypes_TOUCH = 0x8,
};

static inline bool can_see_invisible(VisionTypes vision) {
    // TODO: delete this function in favor of positive ideas like shape and color
    return vision & (VisionTypes_ETHEREAL | VisionTypes_TOUCH);
}
static inline bool can_see_shape(VisionTypes vision) {
    return vision & (VisionTypes_NORMAL | VisionTypes_ETHEREAL | VisionTypes_TOUCH);
}
static inline bool can_see_color(VisionTypes vision) {
    return vision & (VisionTypes_NORMAL | VisionTypes_ETHEREAL);
}
static inline bool can_see_thoughts(VisionTypes vision) {
    // you can only see your own thoughts.
    // TODO: this should really be more like TELEPATHY, but that doesn't exist right now.
    return vision & VisionTypes_TOUCH;
}

enum Mind {
    Mind_NONE,
    Mind_BEAST,
    Mind_SAVAGE,
    Mind_CIVILIZED,
};

// everyone has the same action cost
static const int action_cost = 12;

struct Species {
    // how many ticks does it cost to move one space? average human is 12.
    int movement_cost;
    int base_hitpoints;
    int base_mana;
    int base_attack_power;
    int min_level;
    int max_level;
    Mind mind;
    VisionTypes vision_types;
    bool sucks_up_items;
    bool auto_throws_items;
    bool poison_attack;
};
extern Species specieses[SpeciesId_COUNT];

struct StatusEffect {
    enum Id {
        CONFUSION,
        SPEED,
        ETHEREAL_VISION,
        COGNISCOPY,
        BLINDNESS,
        INVISIBILITY,
        POISON,
    };
    Id type;
    // this is never in the past
    int64_t expiration_time;

    // currently only used for poison
    int64_t poison_next_damage_time;
    // used for awarding experience for poison damage kills
    uint256 who_is_responsible;
};

static inline bool can_see_status_effect(StatusEffect::Id effect, VisionTypes vision) {
    switch (effect) {
        case StatusEffect::CONFUSION: // the derpy look on your face
        case StatusEffect::SPEED:     // the twitchy motion of your body
        case StatusEffect::BLINDNESS: // the empty look in your eyes
        case StatusEffect::POISON:    // the sick look on your face
            return can_see_shape(vision);
        case StatusEffect::INVISIBILITY:
            // ironically, you need normal vision to tell when something can't be seen with it.
            return (vision & VisionTypes_NORMAL);
        case StatusEffect::ETHEREAL_VISION:
        case StatusEffect::COGNISCOPY:
            // you'd have to see things through my eyes
            return can_see_thoughts(vision);
    }
    unreachable();
}
static inline bool can_see_potion_effect(PotionId effect, VisionTypes vision) {
    switch (effect) {
        case PotionId_POTION_OF_HEALING: // the wounds closing
            return can_see_shape(vision);
        case PotionId_POTION_OF_POISON:
            return can_see_status_effect(StatusEffect::POISON, vision);
        case PotionId_POTION_OF_ETHEREAL_VISION:
            return can_see_status_effect(StatusEffect::ETHEREAL_VISION, vision);
        case PotionId_POTION_OF_COGNISCOPY:
            return can_see_status_effect(StatusEffect::COGNISCOPY, vision);
        case PotionId_POTION_OF_BLINDNESS:
            return can_see_status_effect(StatusEffect::BLINDNESS, vision);
        case PotionId_POTION_OF_INVISIBILITY:
            return can_see_status_effect(StatusEffect::INVISIBILITY, vision);

        case PotionId_UNKNOWN: // you alread know you can't see it
            return false;
        case PotionId_COUNT:
            unreachable();
    }
    unreachable();
}

struct Ability {
    enum Id {
        SPIT_BLINDING_VENOM,

        COUNT,
    };
    Id id;
};

struct AbilityCooldown {
    Ability::Id ability_id;
    int64_t expiration_time;
};

struct PerceivedWandInfo {
    WandDescriptionId description_id;
    int used_count; // -1 for empty
};
struct PerceivedPotionInfo {
    PotionDescriptionId description_id;
};
struct PerceivedBookInfo {
    BookDescriptionId description_id;
};
struct PerceivedLife {
    SpeciesId species_id;
};

class PerceivedThingImpl : public ReferenceCounted {
public:
    uint256 id;
    bool is_placeholder;
    ThingType thing_type;
    Coord location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;
    int64_t last_seen_time;
    List<StatusEffect::Id> status_effects;
    // individual
    PerceivedThingImpl(uint256 id, bool is_placeholder, SpeciesId species_id, int64_t last_seen_time) :
            id(id), is_placeholder(is_placeholder), thing_type(ThingType_INDIVIDUAL), last_seen_time(last_seen_time) {
        assert(id != uint256::zero());
        _life = create<PerceivedLife>();
        _life->species_id = species_id;
    }
    // wand
    PerceivedThingImpl(uint256 id, bool is_placeholder, WandDescriptionId description_id, int64_t last_seen_time) :
            id(id), is_placeholder(is_placeholder), thing_type(ThingType_WAND), last_seen_time(last_seen_time) {
        _wand_info = create<PerceivedWandInfo>();
        _wand_info->description_id = description_id;
        _wand_info->used_count = 0;
    }
    // potion
    PerceivedThingImpl(uint256 id, bool is_placeholder, PotionDescriptionId description_id, int64_t last_seen_time) :
            id(id), is_placeholder(is_placeholder), thing_type(ThingType_POTION), last_seen_time(last_seen_time) {
        _potion_info = create<PerceivedPotionInfo>();
        _potion_info->description_id = description_id;
    }
    // book
    PerceivedThingImpl(uint256 id, bool is_placeholder, BookDescriptionId description_id, int64_t last_seen_time) :
            id(id), is_placeholder(is_placeholder), thing_type(ThingType_BOOK), last_seen_time(last_seen_time) {
        _book_info = create<PerceivedBookInfo>();
        _book_info->description_id = description_id;
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
            case ThingType_BOOK:
                destroy(_book_info, 1);
                break;

            case ThingType_COUNT:
                unreachable();
        }
    }
    PerceivedLife * life() {
        assert_str(thing_type == ThingType_INDIVIDUAL, "wrong type");
        return _life;
    }
    PerceivedWandInfo * wand_info() {
        assert_str(thing_type == ThingType_WAND, "wrong type");
        return _wand_info;
    }
    PerceivedPotionInfo * potion_info() {
        assert_str(thing_type == ThingType_POTION, "wrong type");
        return _potion_info;
    }
    PerceivedBookInfo * book_info() {
        assert_str(thing_type == ThingType_BOOK, "wrong type");
        return _book_info;
    }
private:
    union {
        PerceivedLife * _life;
        PerceivedWandInfo * _wand_info;
        PerceivedPotionInfo * _potion_info;
        PerceivedBookInfo * _book_info;
    };
};
typedef Reference<PerceivedThingImpl> PerceivedThing;

struct RememberedEventImpl : public ReferenceCounted {
    Span span = new_span();
};
typedef Reference<RememberedEventImpl> RememberedEvent;

struct Knowledge {
    // terrain knowledge
    MapMatrix<TileType> tiles;
    MapMatrix<VisionTypes> tile_is_visible;
    List<RememberedEvent> remembered_events;
    // incremented whenever events were forgotten, which means we need to blank out and refresh the rendering of the events.
    int event_forget_counter = 0;

    // these are never wrong
    WandId wand_identities[WandDescriptionId_COUNT];
    PotionId potion_identities[PotionDescriptionId_COUNT];
    BookId book_identities[BookDescriptionId_COUNT];

    IdMap<PerceivedThing> perceived_things;
    Knowledge() {
        reset_map();
        for (int i = 0; i < WandDescriptionId_COUNT; i++)
            wand_identities[i] = WandId_UNKNOWN;
        for (int i = 0; i < PotionDescriptionId_COUNT; i++)
            potion_identities[i] = PotionId_UNKNOWN;
        for (int i = 0; i < BookDescriptionId_COUNT; i++)
            book_identities[i] = BookId_UNKNOWN;
    }
    void reset_map() {
        tiles.set_all(TileType_UNKNOWN);
        tile_is_visible.set_all(0);
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
    int64_t hp_regen_deadline;
    int mana;
    int64_t mp_regen_deadline;
    int experience = 0;
    int64_t last_movement_time = 0;
    int64_t last_action_time = 0;
    uint256 initiative;
    DecisionMakerType decision_maker;
    Knowledge knowledge;

    const Species * species() const{
        return &specieses[species_id];
    }
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
    int max_mana() const {
        return species()->base_mana * (experience_level() + 1);
    }
};

struct WandInfo {
    WandId wand_id;
    int charges;
};
struct PotionInfo {
    PotionId potion_id;
};
struct BookInfo {
    BookId book_id;
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
    List<AbilityCooldown> ability_cooldowns;

    // individual
    ThingImpl(SpeciesId species_id, Coord location, DecisionMakerType decision_maker);
    // wand
    ThingImpl(WandId wand_id, int charges);
    // potion
    ThingImpl(PotionId potion_id);
    // book
    ThingImpl(BookId book_id);

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
            case ThingType_BOOK:
                destroy(_book_info, 1);
                break;

            case ThingType_COUNT:
                unreachable();
        }
    }

    Life * life() {
        assert_str(thing_type == ThingType_INDIVIDUAL, "wrong type");
        return _life;
    }
    const Life * life() const {
        assert_str(thing_type == ThingType_INDIVIDUAL, "wrong type");
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
    BookInfo * book_info() {
        if (thing_type != ThingType_BOOK)
            panic("wrong type");
        return _book_info;
    }
private:
    union {
        Life * _life;
        WandInfo * _wand_info;
        PotionInfo * _potion_info;
        BookInfo * _book_info;
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

extern Ability abilities[Ability::COUNT];
extern List<Ability::Id> species_abilities[SpeciesId_COUNT];

static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_individuals(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_individual);
}
static inline FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing> get_perceived_items(Thing individual) {
    return FilteredIterator<IdMap<PerceivedThing>::Iterator, PerceivedThing>(individual->life()->knowledge.perceived_things.value_iterator(), is_item);
}

static inline int compare_individuals_by_initiative(Thing a, Thing b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
static inline int compare_things_by_id(Thing a, Thing b) {
    return compare(a->id, b->id);
}

// TODO: this is in the wrong place
void compute_vision(Thing observer);

static inline bool individual_has_mind(Thing thing) {
    switch (thing->life()->species()->mind) {
        case Mind_NONE:
            return false;
        case Mind_BEAST:
        case Mind_SAVAGE:
        case Mind_CIVILIZED:
            return true;
    }
    unreachable();
}
static inline bool can_have_status(Thing individual, StatusEffect::Id status) {
    switch (status) {
        case StatusEffect::CONFUSION:
            return individual_has_mind(individual);
        case StatusEffect::SPEED:
        case StatusEffect::ETHEREAL_VISION:
        case StatusEffect::COGNISCOPY:
        case StatusEffect::BLINDNESS:
        case StatusEffect::INVISIBILITY:
        case StatusEffect::POISON:
            return true;
    }
    unreachable();
}

static inline int find_status(const List<StatusEffect> & status_effects, StatusEffect::Id status) {
    for (int i = 0; i < status_effects.length(); i++)
        if (status_effects[i].type == status)
            return i;
    return -1;
}
static inline StatusEffect * find_or_put_status(Thing thing, StatusEffect::Id status) {
    int index = find_status(thing->status_effects, status);
    if (index == -1) {
        index = thing->status_effects.length();
        thing->status_effects.append(StatusEffect { status, -1, -1, uint256::zero() });
    }
    return &thing->status_effects[index];
}

static inline bool has_status(const List<StatusEffect> & status_effects, StatusEffect::Id status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status(Thing thing, StatusEffect::Id status) {
    return can_have_status(thing, status) && has_status(thing->status_effects, status);
}
static inline int find_status(const List<StatusEffect::Id> & status_effects, StatusEffect::Id status) {
    for (int i = 0; i < status_effects.length(); i++)
        if (status_effects[i] == status)
            return i;
    return -1;
}
static inline bool has_status(const List<StatusEffect::Id> & status_effects, StatusEffect::Id status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status(PerceivedThing thing, StatusEffect::Id status) {
    return has_status(thing->status_effects, status);
}
static inline void put_status(PerceivedThing thing, StatusEffect::Id status) {
    if (!has_status(thing, status))
        thing->status_effects.append(status);
}
static inline void maybe_remove_status(PerceivedThing thing, StatusEffect::Id status) {
    int index = find_status(thing->status_effects, status);
    if (index != -1)
        thing->status_effects.swap_remove(index);
}

DEFINE_GDB_PY_SCRIPT("debug-scripts/vision_types.py")

#endif
