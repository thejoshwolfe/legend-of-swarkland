#include "individual.hpp"

#include "swarkland.hpp"

ThingImpl::ThingImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker) :
        thing_type(ThingType_INDIVIDUAL), location(location)
{
    id = random_uint256();
    _life = create<Life>();
    _life->species_id = species_id;
    _life->team = team;
    _life->decision_maker = decision_maker;
    _life->hitpoints = _life->max_hitpoints();
    _life->initiative = random_uint256();
}
ThingImpl::ThingImpl(WandDescriptionId description_id, int charges) :
        thing_type(ThingType_WAND)
{
    id = random_uint256();
    _wand_info = create<WandInfo>();
    _wand_info->description_id = description_id;
    _wand_info->charges = charges;
}
ThingImpl::~ThingImpl() {
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            destroy(_life, 1);
            break;
        case ThingType_WAND:
            destroy(_wand_info, 1);
            break;
    }
}


const Species * Life::species() const {
    return &specieses[species_id];
}

PerceivedThing to_perceived_thing(uint256 target_id) {
    Thing target = actual_things.get(target_id);
    StatusEffects status_effects = target->status_effects;
    // nerf some information
    status_effects.confused_expiration_time = status_effects.confused_expiration_time > time_counter ? 0x7fffffffffffffffLL : -1;
    status_effects.speed_up_expiration_time = status_effects.speed_up_expiration_time > time_counter ? 0x7fffffffffffffffLL : -1;
    switch (target->thing_type) {
        case ThingType_INDIVIDUAL:
            return create<PerceivedThingImpl>(target->id, target->life()->species_id, target->location, target->life()->team, status_effects);
        case ThingType_WAND:
            if (target->location != Coord::nowhere())
                return create<PerceivedThingImpl>(target->id, target->wand_info()->description_id, target->location, status_effects);
            else
                return create<PerceivedThingImpl>(target->id, target->wand_info()->description_id, target->container_id, target->z_order, status_effects);
    }
    panic("thing type");
}

PerceivedThing perceive_thing(Thing observer, Thing target) {
    if (target->location != Coord::nowhere()) {
        if (!observer->life()->knowledge.tile_is_visible[target->location].any())
            return nullptr; // standing out of view
    } else {
        PerceivedThing container = perceive_thing(observer, actual_things.get(target->container_id));
        if (container == nullptr)
            return nullptr; // held by something we can't see
    }
    // invisible creates can only be seen by themselves
    if (target->status_effects.invisible && observer != target)
        return nullptr;
    return to_perceived_thing(target->id);
}
