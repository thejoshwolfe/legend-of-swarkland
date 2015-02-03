#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

extern Species specieses[SpeciesId_COUNT];

extern LinkedHashtable<uint256, Individual *, hash_uint256> individuals;
extern Individual * you;
extern long long time_counter;

extern bool cheatcode_full_visibility;
void cheatcode_kill_everybody_in_the_world();
void cheatcode_polymorph();
extern Individual * cheatcode_spectator;
void cheatcode_spectate(Coord individual_at);

void swarkland_init();
void swarkland_finish();

// specify SpeciesId_COUNT for random
Individual * spawn_a_monster(SpeciesId species_id);
void advance_time();
bool take_action(bool just_wait, Coord delta);
Individual * find_individual_at(Coord location);

#endif
