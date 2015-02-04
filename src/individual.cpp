#include "individual.hpp"

#include "swarkland.hpp"

IndividualImpl::IndividualImpl(SpeciesId species_id, Coord location) :
        location(location)
{
    id = random_uint256();
    species = &specieses[species_id];
    hitpoints = species->starting_hitpoints;
    ai = species->default_ai;
}

PerceivedIndividual observe_individual(Individual observer, Individual target) {
    if (!observer->knowledge.tile_is_visible[target->location].any())
        return NULL;
    // invisible creates can only be seen by themselves
    if (target->invisible && observer != target)
        return NULL;
    return new PerceivedIndividualImpl(target->id, target->is_alive, target->species, target->location, target->invisible);
}
