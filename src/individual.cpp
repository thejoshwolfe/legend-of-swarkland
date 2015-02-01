#include "individual.hpp"

#include "swarkland.hpp"

Individual::Individual(SpeciesId species_id, Coord location, AiStrategy ai) :
        location(location), ai(ai) {
    species = specieses[species_id];
    hitpoints = species->starting_hitpoints;
    is_alive = true;
    movement_points = 0;
}
