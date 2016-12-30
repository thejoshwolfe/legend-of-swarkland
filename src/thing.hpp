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
    ThingType_BOOK,

    ThingType_COUNT,
};

enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,
    WandDescriptionId_COPPER_WAND,
    WandDescriptionId_PURPLE_WAND,
    WandDescriptionId_SHINY_BONE_WAND,
    WandDescriptionId_SHINY_GOLD_WAND,
    WandDescriptionId_SHINY_PLASTIC_WAND,
    WandDescriptionId_SHINY_COPPER_WAND,
    WandDescriptionId_SHINY_PURPLE_WAND,

    WandDescriptionId_COUNT,
    WandDescriptionId_UNSEEN,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_MAGIC_MISSILE,
    WandId_WAND_OF_SPEED,
    WandId_WAND_OF_REMEDY,
    WandId_WAND_OF_BLINDING,
    WandId_WAND_OF_FORCE,
    WandId_WAND_OF_INVISIBILITY,
    WandId_WAND_OF_MAGIC_BULLET,
    WandId_WAND_OF_SLOWING,

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
    BookDescriptionId_YELLOW_BOOK,

    BookDescriptionId_COUNT,
    BookDescriptionId_UNSEEN,
};
enum BookId {
    BookId_SPELLBOOK_OF_MAGIC_BULLET,
    BookId_SPELLBOOK_OF_SPEED,
    BookId_SPELLBOOK_OF_MAPPING,
    BookId_SPELLBOOK_OF_FORCE,
    BookId_SPELLBOOK_OF_ASSUME_FORM,

    BookId_COUNT,
    BookId_UNKNOWN,
};

enum SpeciesId {
    SpeciesId_HUMAN,
    SpeciesId_OGRE,
    SpeciesId_LICH,
    SpeciesId_SHAPESHIFTER,
    SpeciesId_PINK_BLOB,
    SpeciesId_AIR_ELEMENTAL,
    SpeciesId_TAR_ELEMENTAL,
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
static const int speedy_movement_cost = 3;
static const int slow_movement_cost = 48;

struct Species {
    SpeciesId species_id;
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
static const VisionTypes _norm = VisionTypes_NORMAL;
static const VisionTypes _ethe = VisionTypes_ETHEREAL;
static const Mind _none = Mind_NONE;
static const Mind _beas = Mind_BEAST;
static const Mind _savg = Mind_SAVAGE;
static const Mind _civl = Mind_CIVILIZED;
static constexpr Species specieses[SpeciesId_COUNT] = {
    //                         movement cost
    //                         |   health
    //                         |   |  base mana
    //                         |   |  |  base attack
    //                         |   |  |  |  min level
    //                         |   |  |  |  |   max level
    //                         |   |  |  |  |   |  mind
    //                         |   |  |  |  |   |  |     vision
    //                         |   |  |  |  |   |  |     |     sucks up items
    //                         |   |  |  |  |   |  |     |     |  auto throws items
    //                         |   |  |  |  |   |  |     |     |  |  poison attack
    {SpeciesId_HUMAN        , 12, 10, 3, 3, 0, 10,_civl,_norm, 0, 0, 0},
    {SpeciesId_OGRE         , 24, 15, 0, 2, 4, 10,_savg,_norm, 0, 0, 0},
    {SpeciesId_LICH         , 12, 12, 4, 3, 7, 10,_civl,_norm, 0, 0, 0},
    {SpeciesId_SHAPESHIFTER , 12,  5, 0, 2, 1, 10,_civl,_norm, 0, 0, 0},
    {SpeciesId_PINK_BLOB    , 48,  4, 0, 1, 0,  1,_none,_ethe, 1, 0, 0},
    {SpeciesId_AIR_ELEMENTAL,  6,  6, 0, 1, 3, 10,_none,_ethe, 1, 1, 0},
    {SpeciesId_TAR_ELEMENTAL, 24, 10, 0, 1, 3, 10,_none,_ethe, 1, 0, 0},
    {SpeciesId_DOG          , 12,  4, 0, 2, 1,  2,_beas,_norm, 0, 0, 0},
    {SpeciesId_ANT          , 12,  2, 0, 1, 0,  1,_beas,_norm, 0, 0, 0},
    {SpeciesId_BEE          , 12,  2, 0, 3, 1,  2,_beas,_norm, 0, 0, 0},
    {SpeciesId_BEETLE       , 24,  6, 0, 1, 0,  1,_beas,_norm, 0, 0, 0},
    {SpeciesId_SCORPION     , 24,  5, 0, 1, 2,  3,_beas,_norm, 0, 0, 1},
    {SpeciesId_SNAKE        , 18,  4, 0, 2, 1,  2,_beas,_norm, 0, 0, 0},
    {SpeciesId_COBRA        , 18,  2, 0, 1, 2,  3,_beas,_norm, 0, 0, 0},
};
static bool constexpr _check_specieses() {
#if __cpp_constexpr >= 201304
    for (int i = 0; i < SpeciesId_COUNT; i++)
        if (specieses[i].species_id != i)
            return false;
#endif
    return true;
}
static_assert(_check_specieses(), "missed a spot");

struct StatusEffect {
    enum Id {
        CONFUSION,
        SPEED,
        ETHEREAL_VISION,
        COGNISCOPY,
        BLINDNESS,
        INVISIBILITY,
        POISON,
        POLYMORPH,
        SLOWING,

        COUNT,
    };
    Id type;
    // this is never in the past
    int64_t expiration_time;

    // TODO: union
    // currently only used for poison
    int64_t poison_next_damage_time;
    // used for awarding experience for poison damage kills
    uint256 who_is_responsible;
    // used for polymorph
    SpeciesId species_id;
};
static inline int compare_status_effects_by_type(StatusEffect a, StatusEffect b) {
    assert_str(a.type != b.type, "status effect list contains duplicates");
    return a.type - b.type;
}

static inline bool can_see_status_effect(StatusEffect::Id effect, VisionTypes vision) {
    switch (effect) {
        case StatusEffect::CONFUSION: // the derpy look on your face
        case StatusEffect::SPEED:     // the twitchy motion of your body
        case StatusEffect::SLOWING:   // the baywatchy motion of your body
        case StatusEffect::BLINDNESS: // the empty look in your eyes
        case StatusEffect::POISON:    // the sick look on your face
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

        case PotionId_UNKNOWN: // you alread know you can't see it
            return false;
        case PotionId_COUNT:
            unreachable();
    }
    unreachable();
}

enum AbilityId {
    AbilityId_SPIT_BLINDING_VENOM,
    AbilityId_THROW_TAR,
    AbilityId_ASSUME_FORM,

    AbilityId_COUNT,
};

struct AbilityCooldown {
    AbilityId ability_id;
    int64_t expiration_time;
};
static inline int compare_ability_cooldowns_by_type(AbilityCooldown a, AbilityCooldown b) {
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
    List<StatusEffect> status_effects;
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

static constexpr int xp_bucket_sizes[] = {2, 4, 6, 6, 6};
enum SkillId {
    SkillId_HP,
    SkillId_MP,
    SkillId_ATTACK_DAMAGE,
    SkillId_DODGE,

    SkillId_COUNT,
};

struct Life {
    SpeciesId original_species_id;
    int hitpoints;
    int64_t hp_regen_deadline;
    int mana;
    int64_t mp_regen_deadline;
    int experience[SkillId_COUNT][get_array_length(xp_bucket_sizes)];
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

class ThingImpl : public ReferenceCounted {
public:
    const uint256 id;
    const ThingType thing_type;
    // this is set to false in the time between actually being destroyed and being removed from the master list
    bool still_exists = true;

    Coord location = Coord::nowhere();
    uint256 container_id = uint256::zero();
    int z_order = 0;

    List<StatusEffect> status_effects;
    List<AbilityCooldown> ability_cooldowns;

    // individual
    ThingImpl(uint256 id, SpeciesId species_id, DecisionMakerType decision_maker, uint256 initiative);
    // wand
    ThingImpl(uint256 id, WandId wand_id, int charges);
    // potion
    ThingImpl(uint256 id, PotionId potion_id);
    // book
    ThingImpl(uint256 id, BookId book_id);

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

    int experience_level(SkillId skill_id) const {
        int total = 0;
        for (int i = 0; i < get_array_length(xp_bucket_sizes); i++)
            total += life()->experience[skill_id][i];
        int expectation = 0;
        int i = 0;
        for (; i < get_array_length(xp_bucket_sizes); i++) {
            expectation += xp_bucket_sizes[i];
            if (total < expectation)
                break;
        }
        return i;
    }
    int highest_skill_level() const {
        int result = 0;
        for (int i = 0; i < SkillId_COUNT; i++) {
            int level = experience_level((SkillId)i);
            if (result < level)
                result = level;
        }
        return result;
    }
    int attack_power() const {
        return max(1, physical_species()->base_attack_power + experience_level(SkillId_ATTACK_DAMAGE) / 2);
    }
    int max_hitpoints() const {
        return physical_species()->base_hitpoints + 2 * experience_level(SkillId_HP);
    }
    int max_mana() const {
        return mental_species()->base_mana * (experience_level(SkillId_MP) + 1);
    }

    int get_combat_difficulty() const {
        if (thing_type != ThingType_INDIVIDUAL)
            return 0;
        return max(physical_species()->min_level, highest_skill_level());
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

static inline int compare_individuals_by_initiative(Thing a, Thing b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
static inline int compare_things_by_id(Thing a, Thing b) {
    return compare(a->id, b->id);
}
static inline int compare_perceived_things_by_id(PerceivedThing a, PerceivedThing b) {
    return compare(a->id, b->id);
}

// TODO: this is in the wrong place
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
static inline bool can_have_status(Thing individual, StatusEffect::Id status) {
    switch (status) {
        case StatusEffect::CONFUSION:
            return individual_has_mind(individual);
        case StatusEffect::SPEED:
            // if you're already moving at the slow speed, you can't tell if it gets worse
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
        case StatusEffect::COGNISCOPY:
        case StatusEffect::INVISIBILITY:
        case StatusEffect::POISON:
        case StatusEffect::POLYMORPH:
            return true;

        case StatusEffect::COUNT:
            unreachable();
    }
    unreachable();
}

static inline StatusEffect * find_or_put_status(Thing thing, StatusEffect::Id status) {
    int index = find_status(thing->status_effects, status);
    if (index == -1) {
        index = thing->status_effects.length();
        thing->status_effects.append(StatusEffect { status, -1, -1, uint256::zero(), SpeciesId_COUNT });
    }
    return &thing->status_effects[index];
}

static inline bool has_status(const List<StatusEffect> & status_effects, StatusEffect::Id status) {
    return find_status(status_effects, status) != -1;
}
static inline bool has_status(Thing thing, StatusEffect::Id status) {
    return can_have_status(thing, status) && has_status(thing->status_effects, status);
}
static inline bool has_status(PerceivedThing thing, StatusEffect::Id status) {
    return has_status(thing->status_effects, status);
}
static inline StatusEffect * find_or_put_status(PerceivedThing thing, StatusEffect::Id status) {
    int index = find_status(thing->status_effects, status);
    if (index == -1) {
        index = thing->status_effects.length();
        thing->status_effects.append(StatusEffect { status, -1, -1, uint256::zero(), SpeciesId_COUNT });
    }
    return &thing->status_effects[index];
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
