#include "thing.hpp"

ThingImpl::ThingImpl(uint256 id, SpeciesId species_id, DecisionMakerType decision_maker, uint256 initiative) :
        id(id), thing_type(ThingType_INDIVIDUAL)
{
    _life = create<Life>();
    _life->original_species_id = species_id;
    _life->decision_maker = decision_maker;
    _life->hitpoints = max_hitpoints();
    _life->mana = max_mana();
    _life->initiative = initiative;
}
ThingImpl::ThingImpl(uint256 id, WandId wand_id, int charges) :
        id(id), thing_type(ThingType_WAND)
{
    _wand_info = create<WandInfo>();
    _wand_info->wand_id = wand_id;
    _wand_info->charges = charges;
}
ThingImpl::ThingImpl(uint256 id, PotionId potion_id) :
        id(id), thing_type(ThingType_POTION)
{
    _potion_info = create<PotionInfo>();
    _potion_info->potion_id = potion_id;
}
ThingImpl::ThingImpl(uint256 id, BookId book_id) :
        id(id), thing_type(ThingType_BOOK)
{
    _book_info = create<BookInfo>();
    _book_info->book_id = book_id;
}
ThingImpl::ThingImpl(uint256 id, WeaponId weapon_id) :
        id(id), thing_type(ThingType_WEAPON)
{
    _weapon_info = create<WeaponInfo>();
    _weapon_info->weapon_id = weapon_id;
}
