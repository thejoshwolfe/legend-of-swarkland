#ifndef ITEM_HPP
#define ITEM_HPP

#include "thing.hpp"

const int beam_length_average = 9;
const int beam_length_error_margin = 3;

const int throw_distance_average = 5;
const int throw_distance_error_margin = 1;
const int infinite_range = max(MAP_SIZE_X, MAP_SIZE_Y);

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

    {POTION_OFFSET + PotionId_POTION_OF_HEALING        , 20},
    {POTION_OFFSET + PotionId_POTION_OF_POISON         , 13},
    {POTION_OFFSET + PotionId_POTION_OF_ETHEREAL_VISION, 16},
    {POTION_OFFSET + PotionId_POTION_OF_COGNISCOPY     ,  8},
    {POTION_OFFSET + PotionId_POTION_OF_BLINDNESS      ,  9},
    {POTION_OFFSET + PotionId_POTION_OF_INVISIBILITY   ,  9},

    {BOOK_OFFSET + BookId_SPELLBOOK_OF_MAGIC_BULLET, 4},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_SPEED       , 3},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_MAPPING     , 2},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_FORCE       , 4},
    {BOOK_OFFSET + BookId_SPELLBOOK_OF_ASSUME_FORM , 1},

    {WEAPON_OFFSET + WeaponId_DAGGER   , 10},
    {WEAPON_OFFSET + WeaponId_BATTLEAXE,  5},
};
static_assert(_check_indexed_array(item_rarities, TOTAL_ITEMS), "missed a spot");

void init_items();
Thing create_random_item();
Thing create_random_item(ThingType thing_type);
Thing create_wand(WandId wand_id);
Thing create_potion(PotionId potion_id);
Thing create_book(BookId book_id);
Thing create_weapon(WeaponId weapon_id);
void delete_item(Thing item);
int get_mana_cost(BookId book_id);
void zap_wand(Thing individual, uint256 item_id, Coord direction);
void read_book(Thing actor, uint256 item_id, Coord direction);
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking);
void explode_wand(Thing actor, Thing item, Coord explosion_center);
void break_potion(Thing actor, Thing item, Coord location);

#endif
