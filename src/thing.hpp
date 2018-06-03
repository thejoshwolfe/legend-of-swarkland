#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "primitives.hpp"
#include "structs.hpp"
#include "reference_counter.hpp"
#include "geometry.hpp"
#include "map.hpp"
#include "hashtable.hpp"
#include "list.hpp"
#include "byte_buffer.hpp"
#include "string.hpp"
#include "text.hpp"
#include "bitfield.hpp"

#include <stdbool.h>

static inline bool can_see_physical_presence(VisionTypes vision) {
    return vision & (VisionTypes_ETHEREAL | VisionTypes_COLOCATION);
}
static inline bool can_see_shape(VisionTypes vision) {
    return vision & (VisionTypes_NORMAL | VisionTypes_ETHEREAL | VisionTypes_COLOCATION);
}
static inline bool can_see_color(VisionTypes vision) {
    return vision & (VisionTypes_NORMAL | VisionTypes_ETHEREAL);
}
static inline bool can_see_thoughts(VisionTypes vision) {
    // you can only see your own thoughts.
    // TODO: this should really be more like TELEPATHY, but that doesn't exist right now.
    return vision & VisionTypes_COLOCATION;
}

static inline int compare_status_effects_by_type(const StatusEffect & a, const StatusEffect & b) {
    assert_str(a.type != b.type, "status effect list contains duplicates");
    return a.type - b.type;
}

static inline bool can_see_status_effect(StatusEffect::Id effect, VisionTypes vision) {
    switch (effect) {
        case StatusEffect::CONFUSION:  // the derpy look on your face
        case StatusEffect::SPEED:      // the twitchy motion of your body
        case StatusEffect::SLOWING:    // the baywatchy motion of your body
        case StatusEffect::BLINDNESS:  // the empty look in your eyes
        case StatusEffect::POISON:     // the sick look on your face
        case StatusEffect::BURROWING:  // the ground rippling beneath your feet
        case StatusEffect::LEVITATING: // the gap between you and the ground
            return can_see_shape(vision);
        case StatusEffect::INVISIBILITY:
            // ironically, you need normal vision to tell when something can't be seen with it.
            return (vision & VisionTypes_NORMAL);
        case StatusEffect::ETHEREAL_VISION:
        case StatusEffect::COGNISCOPY:
            // you'd have to see things through my eyes.
        case StatusEffect::POLYMORPH:
            // the polymorphed status is unknown unless you can see the original mind.
            // otherwise, you just look like the new species.
            return can_see_thoughts(vision);
        case StatusEffect::PUSHED:
            // this is only used to track who is responsible
            return false;

        case StatusEffect::COUNT:
            unreachable();
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
        case PotionId_POTION_OF_BURROWING:
            return can_see_status_effect(StatusEffect::BURROWING, vision);
        case PotionId_POTION_OF_LEVITATION:
            return can_see_status_effect(StatusEffect::LEVITATING, vision);

        case PotionId_UNKNOWN: // you already know you can't see it
            return false;
        case PotionId_COUNT:
            unreachable();
    }
    unreachable();
}

static inline int compare_ability_cooldowns_by_type(const AbilityCooldown & a, const AbilityCooldown & b) {
    assert_str(a.ability_id != b.ability_id, "ability cooldown list contains duplicates");
    return a.ability_id - b.ability_id;
}

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
struct PerceivedWeaponInfo {
    WeaponId weapon_id;
};
struct PerceivedLife {
    SpeciesId species_id;
};

class PerceivedThingImpl : public ReferenceCounted {
public:
    uint256 id;
    bool is_placeholder;
    ThingType thing_type;
    Location location = Location::nowhere();
    int64_t last_seen_time;
    StatusEffectIdBitField status_effect_bits = 0;
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
    // weapon
    PerceivedThingImpl(uint256 id, bool is_placeholder, WeaponId weapon_id, int64_t last_seen_time) :
            id(id), is_placeholder(is_placeholder), thing_type(ThingType_WEAPON), last_seen_time(last_seen_time) {
        _weapon_info = create<PerceivedWeaponInfo>();
        _weapon_info->weapon_id = weapon_id;
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
            case ThingType_WEAPON:
                destroy(_weapon_info, 1);
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
    PerceivedWeaponInfo * weapon_info() {
        assert_str(thing_type == ThingType_WEAPON, "wrong type");
        return _weapon_info;
    }
private:
    union {
        PerceivedLife * _life;
        PerceivedWandInfo * _wand_info;
        PerceivedPotionInfo * _potion_info;
        PerceivedBookInfo * _book_info;
        PerceivedWeaponInfo * _weapon_info;
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
    MapMatrix<uint8_t> aesthetic_indexes;
    MapMatrix<VisionTypes> tile_is_visible;
    List<Nullable<PerceivedEvent>> perceived_events;

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
        aesthetic_indexes.set_all(0);
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
    SpeciesId original_species_id;
    int hitpoints;
    int64_t hp_regen_deadline;
    int mana;
    int64_t mp_regen_deadline;
    int experience = 0;
    int64_t last_movement_time = 0;
    int64_t last_action_time = 0;
    bool can_dodge = false;
    uint256 initiative;
    DecisionMakerType decision_maker;
    Knowledge knowledge;
};

static inline int find_status(const List<StatusEffect> & status_effects, StatusEffect::Id status) {
    for (int i = 0; i < status_effects.length(); i++)
        if (status_effects[i].type == status)
            return i;
    return -1;
}

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
struct WeaponInfo {
    WeaponId weapon_id;
};

class ThingImpl : public ReferenceCounted {
public:
    const uint256 id;
    const ThingType thing_type;
    // this is set to false in the time between actually being destroyed and being removed from the master list
    bool still_exists = true;

    Location location = Location::nowhere();

    List<StatusEffect> status_effects;
    List<AbilityCooldown> ability_cooldowns;

    // individual
    ThingImpl(uint256 id, SpeciesId species_id, DecisionMakerType decision_maker, uint256 initiative) :
        id(id), thing_type(ThingType_INDIVIDUAL)
    {
        _life = create<Life>();
        _life->original_species_id = species_id;
        _life->decision_maker = decision_maker;
        _life->hitpoints = max_hitpoints();
        _life->mana = max_mana();
        _life->initiative = initiative;
    }
    // wand
    ThingImpl(uint256 id, WandId wand_id, int charges) :
        id(id), thing_type(ThingType_WAND)
    {
        _wand_info = create<WandInfo>();
        _wand_info->wand_id = wand_id;
        _wand_info->charges = charges;
    }
    // potion
    ThingImpl(uint256 id, PotionId potion_id) :
        id(id), thing_type(ThingType_POTION)
    {
        _potion_info = create<PotionInfo>();
        _potion_info->potion_id = potion_id;
    }
    // book
    ThingImpl(uint256 id, BookId book_id) :
        id(id), thing_type(ThingType_BOOK)
    {
        _book_info = create<BookInfo>();
        _book_info->book_id = book_id;
    }
    // weapon
    ThingImpl(uint256 id, WeaponId weapon_id) :
        id(id), thing_type(ThingType_WEAPON)
    {
        _weapon_info = create<WeaponInfo>();
        _weapon_info->weapon_id = weapon_id;
    }

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
            case ThingType_WEAPON:
                destroy(_weapon_info, 1);
                break;

            case ThingType_COUNT:
                unreachable();
        }
    }

    Life * life() {
        assert(thing_type == ThingType_INDIVIDUAL);
        return _life;
    }
    const Life * life() const {
        assert(thing_type == ThingType_INDIVIDUAL);
        return _life;
    }
    WandInfo * wand_info() {
        assert(thing_type == ThingType_WAND);
        return _wand_info;
    }
    PotionInfo * potion_info() {
        assert(thing_type == ThingType_POTION);
        return _potion_info;
    }
    BookInfo * book_info() {
        assert(thing_type == ThingType_BOOK);
        return _book_info;
    }
    WeaponInfo * weapon_info() {
        assert(thing_type == ThingType_WEAPON);
        return _weapon_info;
    }

    // these are only valid for individuals
    const Species * mental_species() const {
        return &specieses[life()->original_species_id];
    }
    SpeciesId physical_species_id() const {
        const Life * life = this->life(); // assert now
        int index = find_status(status_effects, StatusEffect::POLYMORPH);
        if (index == -1)
            return life->original_species_id;
        return status_effects[index].species_id;
    }
    const Species * physical_species() const {
        return &specieses[physical_species_id()];
    }
    int experience_level() const {
        return experience_to_level(life()->experience);
    }
    int next_level_up() const {
        return level_to_experience(experience_level() + 1);
    }
    int innate_attack_power() const {
        return max(1, physical_species()->base_attack_power + experience_level() / 2);
    }
    int max_hitpoints() const {
        return physical_species()->base_hitpoints + 2 * experience_level();
    }
    int max_mana() const {
        return mental_species()->base_mana * (experience_level() + 1);
    }
private:
    union {
        Life * _life;
        WandInfo * _wand_info;
        PotionInfo * _potion_info;
        BookInfo * _book_info;
        WeaponInfo * _weapon_info;
    };

    ThingImpl(ThingImpl &) = delete;
    ThingImpl & operator=(ThingImpl &) = delete;
};
typedef Reference<ThingImpl> Thing;

static inline int compare(const Location & a, const Location & b) {
    if (a.type != b.type)
        return compare((int)a.type, (int)b.type);
    int result;
    switch (a.type) {
        case Location::NOWHERE:
            return 0;
        case Location::STANDING:
            return compare(a.standing(), b.standing());
        case Location::FLOOR_PILE:
            if ((result = compare(a.floor_pile().coord, b.floor_pile().coord)) != 0)
                return result;
            return compare(a.floor_pile().z_order, b.floor_pile().z_order);
        case Location::INVENTORY:
            if ((result = compare(a.inventory().container_id, b.inventory().container_id)) != 0)
                return result;
            return compare(a.inventory().z_order, b.inventory().z_order);
    }
    unreachable();
}

static inline int compare_individuals_by_initiative(const Thing & a, const Thing & b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
static inline int compare_things_by_id(const Thing & a, const Thing & b) {
    return compare(a->id, b->id);
}
static inline int compare_perceived_things_by_id(const PerceivedThing & a, const PerceivedThing & b) {
    return compare(a->id, b->id);
}
static inline int compare_things_by_location(const Thing & a, const Thing & b) {
    return compare(a->location, b->location);
}

// TODO: these are in the wrong place
void see_aesthetics(Thing observer);
void compute_vision(Thing observer);

static inline bool individual_has_mind(Thing thing) {
    switch (thing->mental_species()->mind) {
        case Mind_NONE:
            return false;
        case Mind_BEAST:
        case Mind_SAVAGE:
        case Mind_CIVILIZED:
            return true;
    }
    unreachable();
}
static inline bool immune_to_poison(Thing individual) {
    return individual->physical_species()->poison_attack;
}
static inline bool can_have_status(Thing individual, StatusEffect::Id status) {
    switch (status) {
        case StatusEffect::CONFUSION:
            return individual_has_mind(individual);
        case StatusEffect::SPEED:
            // this is everything
            return individual->physical_species()->movement_cost != speedy_movement_cost;
        case StatusEffect::SLOWING:
            // if you're already moving at the slow speed, you can't tell if it gets worse
            return individual->physical_species()->movement_cost != slow_movement_cost;
        case StatusEffect::BLINDNESS:
            // you need normal vision to be blind
            return individual->physical_species()->vision_types & VisionTypes_NORMAL;
        case StatusEffect::ETHEREAL_VISION:
            // if you have ethereal vision anyway, you can't get it again
            return !(individual->physical_species()->vision_types & VisionTypes_ETHEREAL);
        case StatusEffect::LEVITATING:
            return !individual->physical_species()->flying;
        case StatusEffect::POISON:
            return !immune_to_poison(individual);
        case StatusEffect::COGNISCOPY:
        case StatusEffect::INVISIBILITY:
        case StatusEffect::POLYMORPH:
        case StatusEffect::BURROWING:
            return true;
        case StatusEffect::PUSHED:
            // this is only used to track who is responsible
            return false;

        case StatusEffect::COUNT:
            unreachable();
    }
    unreachable();
}

static inline StatusEffect * find_or_put_status(Thing thing, StatusEffect::Id status, int64_t expiration_time) {
    int index = find_status(thing->status_effects, status);
    if (index == -1) {
        index = thing->status_effects.length();
        thing->status_effects.append(StatusEffect { status, expiration_time, -1, uint256::zero(), SpeciesId_COUNT, Coord{0, 0} });
    } else {
        thing->status_effects[index].expiration_time = expiration_time;
    }
    return &thing->status_effects[index];
}

static inline bool has_status(const List<StatusEffect> & status_effects, StatusEffect::Id status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status_internally(Thing thing, StatusEffect::Id status) {
    return has_status(thing->status_effects, status);
}
static inline bool has_status_effectively(Thing thing, StatusEffect::Id status) {
    return can_have_status(thing, status) && has_status_internally(thing, status);
}
static inline bool has_status_apparently(PerceivedThing thing, StatusEffect::Id status) {
    return (thing->status_effect_bits & (1 << status)) != 0;
}
static inline void put_status(PerceivedThing thing, StatusEffect::Id status) {
    thing->status_effect_bits |= 1 << status;
}
static inline void remove_status(PerceivedThing thing, StatusEffect::Id status) {
    thing->status_effect_bits &= ~(1 << status);
}
static inline bool maybe_remove_status(List<StatusEffect> * status_effects, StatusEffect::Id status) {
    int index = find_status(*status_effects, status);
    if (index != -1) {
        status_effects->swap_remove(index);
        return true;
    }
    return false;
}

DEFINE_GDB_PY_SCRIPT("debug-scripts/vision_types.py")

#endif
