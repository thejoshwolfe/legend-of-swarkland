#include "individual.hpp"

#include "swarkland.hpp"

IndividualImpl::IndividualImpl(SpeciesId species_id, Coord location, Team team, DecisionMakerType decision_maker) :
        species_id(species_id), location(location), team(team), decision_maker(decision_maker)
{
    id = random_uint256();
    hitpoints = specieses[species_id].starting_hitpoints;
    initiative = random_uint256();
}

Species * IndividualImpl::species() const {
    return &specieses[species_id];
}

PerceivedIndividual to_perceived_individual(uint256 target_id) {
    Individual target = actual_individuals.get(target_id);
    StatusEffects status_effects = target->status_effects;
    // nerf some information
    status_effects.confused_timeout = !!status_effects.confused_timeout;
    return create<PerceivedIndividualImpl>(target->id, target->species_id, target->location, target->team, status_effects);
}

PerceivedIndividual observe_individual(Individual observer, Individual target) {
    if (!observer->knowledge.tile_is_visible[target->location].any())
        return NULL;
    // invisible creates can only be seen by themselves
    if (target->status_effects.invisible && observer != target)
        return NULL;
    return to_perceived_individual(target->id);
}

int compare_individuals_by_initiative(Individual a, Individual b) {
    return compare(a->initiative, b->initiative);
}
