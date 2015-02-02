#include "individual.hpp"

#include "swarkland.hpp"

Individual::Individual(SpeciesId species_id, Coord location) :
        location(location)
{
    species = &specieses[species_id];
    hitpoints = species->starting_hitpoints;
    ai = species->default_ai;
}
