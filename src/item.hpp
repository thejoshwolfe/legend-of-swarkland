#ifndef ITEM_HPP
#define ITEM_HPP

#include "thing.hpp"

const int beam_length_average = 9;
const int beam_length_error_margin = 3;

static const int infinite_range = max(MAP_SIZE_X, MAP_SIZE_Y);

static const int WAND_OFFSET = 0;
static const int POTION_OFFSET = WAND_OFFSET + WandId_COUNT;
static const int BOOK_OFFSET = POTION_OFFSET + PotionId_COUNT;
static const int WEAPON_OFFSET = BOOK_OFFSET + BookId_COUNT;
static const int TOTAL_ITEMS = WEAPON_OFFSET + WeaponId_COUNT;

static constexpr IndexAndValue<int> item_rarities[TOTAL_ITEMS] = {
    {WAND_OFFSET + WandId_WAND_OF_CONFUSION    , 3},
    {WAND_OFFSET + WandId_WAND_OF_DIGGING      , 5},
    {WAND_OFFSET + WandId_WAND_OF_MAGIC_MISSILE, 3},
    {WAND_OFFSET + WandId_WAND_OF_SPEED        , 3},
    {WAND_OFFSET + WandId_WAND_OF_REMEDY       , 6},
    {WAND_OFFSET + WandId_WAND_OF_BLINDING     , 2},
    {WAND_OFFSET + WandId_WAND_OF_FORCE        , 3},
    {WAND_OFFSET + WandId_WAND_OF_INVISIBILITY , 2},
    {WAND_OFFSET + WandId_WAND_OF_MAGIC_BULLET , 3},
    {WAND_OFFSET + WandId_WAND_OF_SLOWING      , 3},

    {POTION_OFFSET + PotionId_POTION_OF_HEALING        , 19},
    {POTION_OFFSET + PotionId_POTION_OF_POISON         , 12},
    {POTION_OFFSET + PotionId_POTION_OF_ETHEREAL_VISION, 15},
    {POTION_OFFSET + PotionId_POTION_OF_COGNISCOPY     ,  7},
    {POTION_OFFSET + PotionId_POTION_OF_BLINDNESS      ,  8},
    {POTION_OFFSET + PotionId_POTION_OF_INVISIBILITY   ,  8},
    {POTION_OFFSET + PotionId_POTION_OF_BURROWING      ,  8},
    {POTION_OFFSET + PotionId_POTION_OF_LEVITATION     , 10},

    {BOOK_OFFSET + BookId_SPELLBOOK_OF_MAGIC_BULLET, 4},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_SPEED       , 3},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_MAPPING     , 2},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_FORCE       , 4},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_ASSUME_FORM , 1},

    {WEAPON_OFFSET + WeaponId_DAGGER   ,  0},
    {WEAPON_OFFSET + WeaponId_BATTLEAXE,  0},
};
static_assert(_check_indexed_array(item_rarities, TOTAL_ITEMS), "missed a spot");

void init_items();
Thing create_random_item();
Thing create_random_item(ThingType thing_type);
Thing create_wand(WandId wand_id);
Thing create_potion(PotionId potion_id);
Thing create_book(BookId book_id);
Thing create_weapon(WeaponId weapon_id);
int get_mana_cost(BookId book_id);
void zap_wand(Thing individual, uint256 item_id, Coord direction);
void read_book(Thing actor, uint256 item_id, Coord direction);
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking);
void throw_item(Thing actor, Thing item, Coord direction);

template <typename T>
int get_weapon_damage(T item) {
    assert(item->thing_type == ThingType_WEAPON);
    switch (item->weapon_info()->weapon_id) {
        case WeaponId_DAGGER:
            return 1;
        case WeaponId_BATTLEAXE:
            return 2;

        case WeaponId_UNKNOWN:
        case WeaponId_COUNT:
            unreachable();
    }
    unreachable();
}

static const int typical_throw_range_window_min = 4;
static const int typical_throw_range_window_max = 6;

// {min, max} (inclusive)
template <typename T>
Coord get_throw_range_window(T thrown_thing) {
    switch (thrown_thing->thing_type) {
        case ThingType_INDIVIDUAL:
            // can't even hold individuals, let alone throw them.
            unreachable();
        case ThingType_WAND:
        case ThingType_POTION:
        case ThingType_BOOK:
            return Coord{typical_throw_range_window_min, typical_throw_range_window_max};
        case ThingType_WEAPON:
            switch (thrown_thing->weapon_info()->weapon_id) {
                case WeaponId_DAGGER:
                    // range up
                    return Coord{8, 12};
                case WeaponId_BATTLEAXE:
                    // range down
                    return Coord{2, 3};

                case WeaponId_UNKNOWN:
                case WeaponId_COUNT:
                    unreachable();
            }
            unreachable();

        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}

// {min, max} (inclusive)
static inline Coord get_ability_range_window(AbilityId ability_id) {
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            return Coord{typical_throw_range_window_min, typical_throw_range_window_max};
        case AbilityId_THROW_TAR:
            return Coord{typical_throw_range_window_min, typical_throw_range_window_max};
        case AbilityId_ASSUME_FORM:
            return Coord{infinite_range, infinite_range};

        case AbilityId_COUNT:
            unreachable();
    }
    unreachable();
}

// {min, max} (inclusive)
template <typename T>
Coord get_throw_damage_window(T item) {
    switch (item->thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
        case ThingType_POTION:
        case ThingType_BOOK:
            // token damage
            return Coord{1, 2};
        case ThingType_WEAPON:
            switch (item->weapon_info()->weapon_id) {
                case WeaponId_DAGGER:
                    // more damage
                    return Coord{2, 4};
                case WeaponId_BATTLEAXE:
                    // lots damage
                    return Coord{3, 6};

                case WeaponId_UNKNOWN:
                case WeaponId_COUNT:
                    unreachable();
            }
            unreachable();

        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}

#endif
