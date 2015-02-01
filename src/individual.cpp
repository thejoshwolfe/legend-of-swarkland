#include "individual.hpp"

#include "swarkland.hpp"

Individual::Individual(SpeciesId species_id, Coord location) :
        location(location) {
    species = specieses[species_id];
    hitpoints = species->starting_hitpoints;
    is_alive = true;
    movement_points = 0;
    kill_counter = 0;
    ai = species->default_ai;
}
