#ifndef STRUCTS_HPP
#define STRUCTS_HPP

#include "primitives.hpp"
#include "geometry.hpp"
#include "hashtable.hpp"
#include "bitfield.hpp"

// The structs defined in this file should be copyable, pointerless blobs of data.

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
        BURROWING,
        LEVITATING,
        PUSHED,

        COUNT,
    };
    Id type;
    // this is never in the past
    int64_t expiration_time;

    int64_t poison_next_damage_time;
    // used for awarding experience
    uint256 who_is_responsible;
    // used for polymorph
    SpeciesId species_id;
    // used for levitation momentum
    Coord coord;
};

using StatusEffectIdBitField = BitField<StatusEffect::COUNT - 1>::Type;

struct Event {
    enum Type {
        THE_INDIVIDUAL,
        THE_LOCATION,
        INDIVIDUAL_AND_STATUS,
        INDIVIDUAL_AND_LOCATION,
        INDIVIDUAL_AND_TWO_LOCATION,
        TWO_INDIVIDUAL,
        INDIVIDUAL_AND_ITEM,
        POLYMORPH,
        ITEM_AND_LOCATION,
    };
    Type type;

    enum IndividualAndLocationDataId {
        BUMP_INTO_LOCATION,
        ATTACK_LOCATION,
        INDIVIDUAL_BURROWS_THROUGH_WALL,
    };
    struct IndividualAndLocationData {
        IndividualAndLocationDataId id;
        uint256 actor;
        Coord location;
        bool is_air;
    };

    enum IndividualAndTwoLocationDataId {
        MOVE,
    };
    struct IndividualAndTwoLocationData {
        uint256 actor;
        Coord old_location;
        Coord new_location;
    };

    enum TwoIndividualDataId {
        BUMP_INTO_INDIVIDUAL,
        ATTACK_INDIVIDUAL,
        DODGE_ATTACK,
        MELEE_KILL,
    };
    struct TwoIndividualData {
        TwoIndividualDataId id;
        uint256 actor;
        uint256 target;
    };

    enum IndividualAndItemDataId {
        ZAP_WAND,
        ZAP_WAND_NO_CHARGES,
        WAND_DISINTEGRATES,
        READ_BOOK,
        THROW_ITEM,
        ITEM_HITS_INDIVIDUAL,
        ITEM_SINKS_INTO_INDIVIDUAL,
        INDIVIDUAL_DODGES_THROWN_ITEM,
        INDIVIDUAL_PICKS_UP_ITEM,
        INDIVIDUAL_SUCKS_UP_ITEM,
        QUAFF_POTION,
        POTION_HITS_INDIVIDUAL,
    };
    struct IndividualAndItemData {
        IndividualAndItemDataId id;
        uint256 individual;
        uint256 item;
    };

    enum ItemAndLocationDataId {
        WAND_EXPLODES,
        ITEM_HITS_WALL,
        ITEM_DROPS_TO_THE_FLOOR,
        POTION_BREAKS,
        ITEM_DISINTEGRATES_IN_LAVA,
    };
    struct ItemAndLocationData {
        ItemAndLocationDataId id;
        uint256 item;
        Coord location;
    };

    enum TheIndividualDataId {
        APPEAR,
        LEVEL_UP,
        DIE,
        DELETE_THING,
        SPIT_BLINDING_VENOM,
        BLINDING_VENOM_HIT_INDIVIDUAL,
        THROW_TAR,
        TAR_HIT_INDIVIDUAL,
        MAGIC_BEAM_HIT_INDIVIDUAL,
        INDIVIDUAL_DODGES_MAGIC_BEAM,
        MAGIC_MISSILE_HIT_INDIVIDUAL,
        MAGIC_BULLET_HIT_INDIVIDUAL,
        INDIVIDUAL_IS_HEALED,
        ACTIVATED_MAPPING,
        MAGIC_BEAM_PUSH_INDIVIDUAL,
        MAGIC_BEAM_RECOILS_AND_PUSHES_INDIVIDUAL,
        FAIL_TO_CAST_SPELL,
        SEARED_BY_LAVA,
        INDIVIDUAL_FLOATS_UNCONTROLLABLY,
        LUNGE,
    };
    struct TheIndividualData {
        TheIndividualDataId id;
        uint256 individual;
    };

    enum TheLocationDataId {
        MAGIC_BEAM_HIT_WALL,
        BEAM_OF_DIGGING_DIGS_WALL,
        MAGIC_BEAM_PASS_THROUGH_AIR,
    };
    struct TheLocationData {
        TheLocationDataId id;
        Coord location;
    };

    enum IndividualAndStatusDataId {
        GAIN_STATUS,
        LOSE_STATUS,
    };
    struct IndividualAndStatusData {
        IndividualAndStatusDataId id;
        uint256 individual;
        StatusEffect::Id status;
    };

    TheIndividualData & the_individual_data() {
        check_data_type(THE_INDIVIDUAL);
        return _data._the_individual;
    }

    TheLocationData & the_location_data() {
        check_data_type(THE_LOCATION);
        return _data._the_location;
    }

    IndividualAndStatusData & individual_and_status_data() {
        check_data_type(INDIVIDUAL_AND_STATUS);
        return _data._individual_and_status;
    }

    IndividualAndLocationData & individual_and_location_data() {
        check_data_type(INDIVIDUAL_AND_LOCATION);
        return _data._individual_and_location;
    }

    IndividualAndTwoLocationData & individual_and_two_location_data() {
        check_data_type(INDIVIDUAL_AND_TWO_LOCATION);
        return _data._individual_and_two_location;
    }

    TwoIndividualData & two_individual_data() {
        check_data_type(TWO_INDIVIDUAL);
        return _data._two_individual;
    }

    IndividualAndItemData & individual_and_item_data() {
        check_data_type(INDIVIDUAL_AND_ITEM);
        return _data._individual_and_item;
    }

    struct PolymorphData {
        uint256 individual;
        SpeciesId new_species;
    };
    PolymorphData & polymorph_data() {
        check_data_type(POLYMORPH);
        return _data._polymorph;
    };

    ItemAndLocationData & item_and_location_data() {
        check_data_type(ITEM_AND_LOCATION);
        return _data._item_and_location;
    }

    Event() {}
    static inline Event polymorph(uint256 shapeshifter, SpeciesId new_species) {
        Event result;
        result.type = POLYMORPH;
        PolymorphData & data = result.polymorph_data();
        data.individual = shapeshifter;
        data.new_species = new_species;
        return result;
    }

    Event(TheIndividualDataId id, uint256 individual_id) : type(THE_INDIVIDUAL) {
        the_individual_data() = {
            id,
            individual_id,
        };
    }

    Event(TheLocationDataId id, Coord location) : type(THE_LOCATION) {
        the_location_data() = {
            id,
            location,
        };
    }

    Event(IndividualAndStatusDataId id, uint256 individual_id, StatusEffect::Id status) : type(INDIVIDUAL_AND_STATUS) {
        individual_and_status_data() = {
            id,
            individual_id,
            status,
        };
    }

    Event(IndividualAndLocationDataId id, uint256 actor, Coord location, bool is_air) : type(INDIVIDUAL_AND_LOCATION) {
        individual_and_location_data() = {
            id,
            actor,
            location,
            is_air,
        };
    }

    Event(IndividualAndTwoLocationDataId, uint256 actor, Coord old_location, Coord new_location) : type(INDIVIDUAL_AND_TWO_LOCATION) {
        individual_and_two_location_data() = {
            actor,
            old_location,
            new_location,
        };
    }

    Event(TwoIndividualDataId id, uint256 actor, uint256 target) : type(TWO_INDIVIDUAL) {
        two_individual_data() = {
            id,
            actor,
            target,
        };
    }

    Event(IndividualAndItemDataId id, uint256 individual_id, uint256 item_id) : type(INDIVIDUAL_AND_ITEM) {
        individual_and_item_data() = {
            id,
            individual_id,
            item_id,
        };
    }

    Event(ItemAndLocationDataId id, uint256 item, Coord location) : type(ITEM_AND_LOCATION) {
        item_and_location_data() = {
            id,
            item,
            location,
        };
    }

    union {
        TheIndividualData _the_individual;
        TheLocationData _the_location;
        IndividualAndStatusData _individual_and_status;
        IndividualAndLocationData _individual_and_location;
        IndividualAndTwoLocationData _individual_and_two_location;
        TwoIndividualData _two_individual;
        IndividualAndItemData _individual_and_item;
        PolymorphData _polymorph;
        ItemAndLocationData _item_and_location;
    } _data;

    void check_data_type(Type supposed_data_type) const {
        assert_str(supposed_data_type == type, "wrong data type");
    }
};

struct PerceivedEvent {
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
    bool flying;
};
static const Mind _none = Mind_NONE;
static const Mind _beas = Mind_BEAST;
static const Mind _savg = Mind_SAVAGE;
static const Mind _civl = Mind_CIVILIZED;
static const VisionTypes _norm = VisionTypes_NORMAL;
static const VisionTypes _ethe = VisionTypes_ETHEREAL;
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
    //                         |   |  |  |  |   |  |     |     |  |  |  flying
    {SpeciesId_HUMAN        , 12, 10, 3, 3, 0, 10,_civl,_norm, 0, 0, 0, 0},
    {SpeciesId_OGRE         , 24, 15, 0, 2, 4, 10,_savg,_norm, 0, 0, 0, 0},
    {SpeciesId_LICH         , 12, 12, 4, 3, 7, 10,_civl,_norm, 0, 0, 0, 0},
    {SpeciesId_SHAPESHIFTER , 12,  5, 0, 2, 1, 10,_civl,_norm, 0, 0, 0, 0},
    {SpeciesId_PINK_BLOB    , 48,  4, 0, 1, 0,  1,_none,_ethe, 1, 0, 0, 0},
    {SpeciesId_AIR_ELEMENTAL,  6,  6, 0, 1, 3, 10,_none,_ethe, 1, 1, 0, 1},
    {SpeciesId_TAR_ELEMENTAL, 24, 10, 0, 1, 3, 10,_none,_ethe, 1, 0, 0, 0},
    {SpeciesId_DOG          , 12,  4, 0, 2, 1,  2,_beas,_norm, 0, 0, 0, 0},
    {SpeciesId_ANT          , 12,  2, 0, 1, 0,  1,_beas,_norm, 0, 0, 0, 0},
    {SpeciesId_BEE          , 12,  2, 0, 3, 1,  2,_beas,_norm, 0, 0, 0, 1},
    {SpeciesId_BEETLE       , 24,  6, 0, 1, 0,  1,_beas,_norm, 0, 0, 0, 0},
    {SpeciesId_SCORPION     , 24,  5, 0, 1, 2,  3,_beas,_norm, 0, 0, 1, 0},
    {SpeciesId_SNAKE        , 24,  4, 0, 2, 1,  2,_beas,_norm, 0, 0, 0, 0},
    {SpeciesId_COBRA        , 24,  2, 0, 1, 2,  3,_beas,_norm, 0, 0, 0, 0},
};
#if __cpp_constexpr >= 201304
static bool constexpr _check_specieses() {
    for (int i = 0; i < SpeciesId_COUNT; i++)
        if (specieses[i].species_id != i)
            return false;
    return true;
}
static_assert(_check_specieses(), "missed a spot");
#endif

struct AbilityCooldown {
    AbilityId ability_id;
    int64_t expiration_time;
};

#endif
