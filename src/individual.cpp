#include "individual.hpp"

#include "swarkland.hpp"

ThingImpl::ThingImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker) :
        species_id(species_id), location(location)
{
    id = random_uint256();
    _life = create<Life>();
    _life->team = team;
    _life->decision_maker = decision_maker;
    _life->hitpoints = specieses[species_id].starting_hitpoints;
    _life->initiative = random_uint256();
}
ThingImpl::~ThingImpl() {
    destroy(_life, 1);
}

Species * ThingImpl::species() const {
    return &specieses[species_id];
}

PerceivedThing to_perceived_individual(uint256 target_id) {
    Thing target = actual_individuals.get(target_id);
    StatusEffects status_effects = target->status_effects;
    // nerf some information
    status_effects.confused_timeout = !!status_effects.confused_timeout;
    return create<PerceivedIndividualImpl>(target->id, target->species_id, target->location, target->life()->team, status_effects);
}

PerceivedThing observe_individual(Thing observer, Thing target) {
    if (!observer->life()->knowledge.tile_is_visible[target->location].any())
        return NULL;
    // invisible creates can only be seen by themselves
    if (target->status_effects.invisible && observer != target)
        return NULL;
    return to_perceived_individual(target->id);
}

int compare_individuals_by_initiative(Thing a, Thing b) {
    return compare(a->life()->initiative, b->life()->initiative);
}
