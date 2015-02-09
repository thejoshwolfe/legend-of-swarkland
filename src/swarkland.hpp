#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "action.hpp"

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Individual> actual_individuals;
extern Individual you;
extern bool youre_still_alive;
extern long long time_counter;

extern bool cheatcode_full_visibility;
extern Individual cheatcode_spectator;
void cheatcode_spectate();

void swarkland_init();

void get_available_actions(Individual individual, List<Action> & output_actions);

Individual spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker);
void run_the_game();
PerceivedIndividual find_perceived_individual_at(Individual observer, Coord location);
Individual find_individual_at(Coord location);

bool confuse_individual(Individual target);
void strike_individual(Individual attacker, Individual target);

void change_map(Coord location, TileType new_tile_type);

#endif
