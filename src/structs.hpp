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

template<typename ThingType>
struct EventBase {
    enum Type {
        THE_INDIVIDUAL,
        THE_LOCATION,
        INDIVIDUAL_AND_STATUS,
        INDIVIDUAL_AND_LOCATION,
        INDIVIDUAL_AND_TWO_LOCATION,
        TWO_INDIVIDUAL,
        INDIVIDUAL_AND_ITEM,
        INDIVIDUAL_AND_TWO_ITEM,
        INDIVIDUAL_AND_SPECIES,
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
        ThingType actor;
        Coord location;
        bool is_air;
    };
    IndividualAndLocationData & individual_and_location_data() {
        check_data_type(INDIVIDUAL_AND_LOCATION);
        return _data._individual_and_location;
    }
    static EventBase create(IndividualAndLocationDataId id, ThingType actor, Coord location, bool is_air) {
        EventBase result;
        result.type = INDIVIDUAL_AND_LOCATION;
        result.individual_and_location_data() = {
            id,
            actor,
            location,
            is_air,
        };
        return result;
    }

    enum IndividualAndTwoLocationDataId {
        MOVE,
    };
    struct IndividualAndTwoLocationData {
        ThingType actor;
        Coord old_location;
        Coord new_location;
    };
    IndividualAndTwoLocationData & individual_and_two_location_data() {
        check_data_type(INDIVIDUAL_AND_TWO_LOCATION);
        return _data._individual_and_two_location;
    }
    static EventBase create(IndividualAndTwoLocationDataId, ThingType actor, Coord old_location, Coord new_location) {
        EventBase result;
        result.type = INDIVIDUAL_AND_TWO_LOCATION;
        result.individual_and_two_location_data() = {
            actor,
            old_location,
            new_location,
        };
        return result;
    }

    enum TwoIndividualDataId {
        BUMP_INTO_INDIVIDUAL,
        ATTACK_INDIVIDUAL,
        DODGE_ATTACK,
        MELEE_KILL,
        PUSH_INDIVIDUAL,
    };
    struct TwoIndividualData {
        TwoIndividualDataId id;
        ThingType actor;
        ThingType target;
    };
    TwoIndividualData & two_individual_data() {
        check_data_type(TWO_INDIVIDUAL);
        return _data._two_individual;
    }
    static EventBase create(TwoIndividualDataId id, ThingType actor, ThingType target) {
        EventBase result;
        result.type = TWO_INDIVIDUAL;
        result.two_individual_data() = {
            id,
            actor,
            target,
        };
        return result;
    }

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
        EQUIP_ITEM,
        UNEQUIP_ITEM,
    };
    struct IndividualAndItemData {
        IndividualAndItemDataId id;
        ThingType individual;
        ThingType item;
    };
    IndividualAndItemData & individual_and_item_data() {
        check_data_type(INDIVIDUAL_AND_ITEM);
        return _data._individual_and_item;
    }
    static EventBase create(IndividualAndItemDataId id, ThingType individual, ThingType item) {
        EventBase result;
        result.type = INDIVIDUAL_AND_ITEM;
        result.individual_and_item_data() = {
            id,
            individual,
            item,
        };
        return result;
    }

    enum IndividualAndTwoItemDataId {
        SWAP_EQUIPPED_ITEM,
    };
    struct IndividualAndTwoItemData {
        IndividualAndTwoItemDataId id;
        ThingType individual;
        ThingType old_item;
        ThingType new_item;
    };
    IndividualAndTwoItemData & individual_and_two_item_data() {
        check_data_type(INDIVIDUAL_AND_TWO_ITEM);
        return _data._individual_and_two_item;
    }
    static EventBase create(IndividualAndTwoItemDataId id, ThingType individual, ThingType old_item, ThingType new_item) {
        EventBase result;
        result.type = INDIVIDUAL_AND_TWO_ITEM;
        result.individual_and_two_item_data() = {
            id,
            individual,
            old_item,
            new_item,
        };
        return result;
    }

    enum ItemAndLocationDataId {
        WAND_EXPLODES,
        ITEM_HITS_WALL,
        ITEM_DROPS_TO_THE_FLOOR,
        POTION_BREAKS,
        ITEM_DISINTEGRATES_IN_LAVA,
    };
    struct ItemAndLocationData {
        ItemAndLocationDataId id;
        ThingType item;
        Coord location;
    };
    ItemAndLocationData & item_and_location_data() {
        check_data_type(ITEM_AND_LOCATION);
        return _data._item_and_location;
    }
    static EventBase create(ItemAndLocationDataId id, ThingType item, Coord location) {
        EventBase result;
        result.type = ITEM_AND_LOCATION;
        result.item_and_location_data() = {
            id,
            item,
            location,
        };
        return result;
    }

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
        ThingType individual;
    };
    TheIndividualData & the_individual_data() {
        check_data_type(THE_INDIVIDUAL);
        return _data._the_individual;
    }
    static EventBase create(TheIndividualDataId id, ThingType individual_id) {
        EventBase result;
        result.type = THE_INDIVIDUAL;
        result.the_individual_data() = {
            id,
            individual_id,
        };
        return result;
    }

    enum TheLocationDataId {
        MAGIC_BEAM_HIT_WALL,
        BEAM_OF_DIGGING_DIGS_WALL,
        MAGIC_BEAM_PASS_THROUGH_AIR,
    };
    struct TheLocationData {
        TheLocationDataId id;
        Coord location;
    };
    TheLocationData & the_location_data() {
        check_data_type(THE_LOCATION);
        return _data._the_location;
    }
    static EventBase create(TheLocationDataId id, Coord location) {
        EventBase result;
        result.type = THE_LOCATION;
        result.the_location_data() = {
            id,
            location,
        };
        return result;
    }

    enum IndividualAndStatusDataId {
        GAIN_STATUS,
        LOSE_STATUS,
    };
    struct IndividualAndStatusData {
        IndividualAndStatusDataId id;
        ThingType individual;
        StatusEffect::Id status;
    };
    IndividualAndStatusData & individual_and_status_data() {
        check_data_type(INDIVIDUAL_AND_STATUS);
        return _data._individual_and_status;
    }
    static EventBase create(IndividualAndStatusDataId id, ThingType individual_id, StatusEffect::Id status) {
        EventBase result;
        result.type = INDIVIDUAL_AND_STATUS;
        result.individual_and_status_data() = {
            id,
            individual_id,
            status,
        };
        return result;
    }

    enum IndividualAndSpeciesDataId {
        POLYMORPH,
    };
    struct IndividualAndSpeciesData {
        ThingType individual;
        SpeciesId new_species;
    };
    IndividualAndSpeciesData & individual_and_species_data() {
        check_data_type(INDIVIDUAL_AND_SPECIES);
        return _data._individual_and_species;
    };
    static EventBase create(IndividualAndSpeciesDataId, ThingType shapeshifter, SpeciesId new_species){
        EventBase result;
        result.type = INDIVIDUAL_AND_SPECIES;
        result.individual_and_species_data() = {
            shapeshifter,
            new_species,
        };
        return result;
    }

private:
    union {
        TheIndividualData _the_individual;
        TheLocationData _the_location;
        IndividualAndStatusData _individual_and_status;
        IndividualAndLocationData _individual_and_location;
        IndividualAndTwoLocationData _individual_and_two_location;
        TwoIndividualData _two_individual;
        IndividualAndItemData _individual_and_item;
        IndividualAndTwoItemData _individual_and_two_item;
        IndividualAndSpeciesData _individual_and_species;
        ItemAndLocationData _item_and_location;
    } _data;

    void check_data_type(Type supposed_data_type) const {
        assert_str(supposed_data_type == type, "wrong data type");
    }
};


enum ThingSnapshotType {
    ThingSnapshotType_YOU,
    ThingSnapshotType_INDIVIDUAL,
    ThingSnapshotType_WAND,
    ThingSnapshotType_POTION,
    ThingSnapshotType_BOOK,
    ThingSnapshotType_WEAPON,
};
struct ThingSnapshot {
    ThingSnapshotType thing_type;

    bool is_you() {
        return thing_type == ThingSnapshotType_YOU;
    }
    static ThingSnapshot create_you() {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_YOU;
        return result;
    }

    struct IndividualSnapshotData {
        SpeciesId species_id;
        StatusEffectIdBitField status_effect_bits;
    };
    IndividualSnapshotData & individual_data() {
        assert(thing_type == ThingSnapshotType_INDIVIDUAL);
        return _data._individual_data;
    }
    static ThingSnapshot create(SpeciesId species_id, StatusEffectIdBitField status_effect_bits) {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_INDIVIDUAL;
        result.individual_data() = {
            species_id,
            status_effect_bits,
        };
        return result;
    }

    struct WandSnapshotData {
        WandDescriptionId description_id;
        WandId identified_id;
    };
    WandSnapshotData & wand_data() {
        assert(thing_type == ThingSnapshotType_WAND);
        return _data._wand_data;
    }
    static ThingSnapshot create(WandDescriptionId description_id, WandId identified_id) {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_WAND;
        result.wand_data() = {
            description_id,
            identified_id,
        };
        return result;
    }

    struct PotionSnapshotData {
        PotionDescriptionId description_id;
        PotionId identified_id;
    };
    PotionSnapshotData & potion_data() {
        assert(thing_type == ThingSnapshotType_POTION);
        return _data._potion_data;
    }
    static ThingSnapshot create(PotionDescriptionId description_id, PotionId identified_id) {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_POTION;
        result.potion_data() = {
            description_id,
            identified_id,
        };
        return result;
    }

    struct BookSnapshotData {
        BookDescriptionId description_id;
        BookId identified_id;
    };
    BookSnapshotData & book_data() {
        assert(thing_type == ThingSnapshotType_BOOK);
        return _data._book_data;
    }
    static ThingSnapshot create(BookDescriptionId description_id, BookId identified_id) {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_BOOK;
        result.book_data() = {
            description_id,
            identified_id,
        };
        return result;
    }

    WeaponId & weapon_data() {
        assert(thing_type == ThingSnapshotType_WEAPON);
        return _data._weapon_data;
    }
    static ThingSnapshot create(WeaponId weapon_id) {
        ThingSnapshot result;
        result.thing_type = ThingSnapshotType_WEAPON;
        result.weapon_data() = weapon_id;
        return result;
    }

private:
    union {
        IndividualSnapshotData _individual_data;
        WandSnapshotData _wand_data;
        PotionSnapshotData _potion_data;
        BookSnapshotData _book_data;
        WeaponId _weapon_data;
    } _data;
};

typedef EventBase<uint256> Event;
typedef EventBase<ThingSnapshot> PerceivedEvent;

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

struct WeaponBehavior {
    enum Type {
        BONUS_DAMAGE,
        PUSH,
    };
    Type type;
    int bonus_damage;

    static WeaponBehavior create_bonus_damage(int bonus_damage) {
        WeaponBehavior result;
        result.type = BONUS_DAMAGE;
        result.bonus_damage = bonus_damage;
        return result;
    }
    static WeaponBehavior create_push() {
        WeaponBehavior result;
        result.type = PUSH;
        result.bonus_damage = 0;
        return result;
    }
};

struct FloorPileLocation {
    Coord coord;
    int z_order;
};
struct InventoryLocation {
    uint256 container_id;
    int z_order;
    bool is_equipped;
};

struct Location {
    enum Type {
        NOWHERE,
        STANDING, // or flying, slithering, etc.
        FLOOR_PILE,
        INVENTORY,
    };
    Type type;

    static Location nowhere() {
        Location result;
        result.type = NOWHERE;
        return result;
    }

    Coord & standing() {
        assert(type == STANDING);
        return _data._standing;
    }
    const Coord & standing() const{
        assert(type == STANDING);
        return _data._standing;
    }
    static Location create_standing(Coord coord) {
        Location result;
        result.type = STANDING;
        result.standing() = coord;
        return result;
    }

    FloorPileLocation & floor_pile() {
        assert(type == FLOOR_PILE);
        return _data._floor_pile;
    }
    const FloorPileLocation & floor_pile() const {
        assert(type == FLOOR_PILE);
        return _data._floor_pile;
    }
    static Location create_floor_pile(Coord coord, int z_order) {
        Location result;
        result.type = FLOOR_PILE;
        result.floor_pile() = {
            coord,
            z_order,
        };
        return result;
    }

    InventoryLocation & inventory() {
        assert(type == INVENTORY);
        return _data._inventory;
    }
    const InventoryLocation & inventory() const {
        assert(type == INVENTORY);
        return _data._inventory;
    }
    static Location create_inventory(uint256 container_id, int z_order, bool equipped) {
        Location result;
        result.type = INVENTORY;
        result.inventory() = {
            container_id,
            z_order,
            equipped,
        };
        return result;
    }

    union {
        Coord _standing;
        FloorPileLocation _floor_pile;
        InventoryLocation _inventory;
    } _data;
};

#endif
