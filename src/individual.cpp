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
