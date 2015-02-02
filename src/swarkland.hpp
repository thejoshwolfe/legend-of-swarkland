#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

extern Species * specieses[SpeciesId_COUNT];

extern List<Individual *> individuals;
extern Individual * you;
extern long long time_counter;

extern bool cheatcode_full_visibility;
void cheatcode_kill_everybody_in_the_world();
void cheatcode_polymorph();

void swarkland_init();
void swarkland_finish();

void advance_time();
bool take_action(bool just_wait, Coord delta);
Individual * find_individual_at(Coord location);

#endif
