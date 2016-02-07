#include "thing.hpp"

#include "swarkland.hpp"

static uint256 random_arbitrary_large_number_count;
uint256 random_id() {
    if (test_mode) {
        // just increment a counter
        random_arbitrary_large_number_count.values[3]++;
        return random_arbitrary_large_number_count;
    }
    return random_uint256();
}
static uint256 random_initiative_count;
uint256 random_initiative() {
    if (test_mode) {
        // just increment a counter
        random_initiative_count.values[3]++;
        return random_initiative_count;
    }
    return random_uint256();
}

ThingImpl::ThingImpl(SpeciesId species_id, Coord location, DecisionMakerType decision_maker) :
        thing_type(ThingType_INDIVIDUAL), location(location)
{
    id = random_id();
    _life = create<Life>();
    _life->original_species_id = species_id;
    _life->decision_maker = decision_maker;
    _life->hitpoints = max_hitpoints();
    _life->mana = max_mana();
    _life->initiative = random_initiative();
}
ThingImpl::ThingImpl(WandId wand_id, int charges) :
        thing_type(ThingType_WAND)
{
    id = random_id();
    _wand_info = create<WandInfo>();
    _wand_info->wand_id = wand_id;
    _wand_info->charges = charges;
}
ThingImpl::ThingImpl(PotionId potion_id) :
    thing_type(ThingType_POTION)
{
    id = random_id();
    _potion_info = create<PotionInfo>();
    _potion_info->potion_id = potion_id;
}
ThingImpl::ThingImpl(BookId book_id) :
    thing_type(ThingType_BOOK)
{
    id = random_id();
    _book_info = create<BookInfo>();
    _book_info->book_id = book_id;
}
