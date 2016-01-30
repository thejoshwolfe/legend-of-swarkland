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
    _life->species_id = species_id;
    _life->decision_maker = decision_maker;
    _life->hitpoints = _life->max_hitpoints();
    _life->initiative = random_initiative();
}
ThingImpl::ThingImpl(WandDescriptionId description_id, int charges) :
        thing_type(ThingType_WAND)
{
    id = random_id();
    _wand_info = create<WandInfo>();
    _wand_info->description_id = description_id;
    _wand_info->charges = charges;
}
ThingImpl::ThingImpl(PotionDescriptionId description_id) :
    thing_type(ThingType_POTION)
{
    id = random_id();
    _potion_info = create<PotionInfo>();
    _potion_info->description_id = description_id;
}


const Species * Life::species() const {
    return &specieses[species_id];
}
