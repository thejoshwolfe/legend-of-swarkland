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

PerceivedIndividual to_perceived_individual(Individual target) {
    return new PerceivedIndividualImpl(target->id, target->species_id, target->location, target->team, target->invisible);
}

PerceivedIndividual observe_individual(Individual observer, Individual target) {
    if (!observer->knowledge.tile_is_visible[target->location].any())
        return NULL;
    // invisible creates can only be seen by themselves
    if (target->invisible && observer != target)
        return NULL;
    return to_perceived_individual(target);
}

int compare_individuals_by_initiative(Individual a, Individual b) {
    return compare(a->initiative, b->initiative);
}
