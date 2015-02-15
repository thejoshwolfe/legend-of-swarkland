#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "action.hpp"

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Thing> actual_individuals;
extern IdMap<Item> actual_items;
extern Thing you;
extern bool youre_still_alive;
extern long long time_counter;

extern bool cheatcode_full_visibility;
extern Thing cheatcode_spectator;
void cheatcode_spectate();

void swarkland_init();

void get_available_actions(Thing individual, List<Action> & output_actions);

Thing spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker);
void run_the_game();
PerceivedThing find_perceived_individual_at(Thing observer, Coord location);
Thing find_individual_at(Coord location);
void find_items_in_inventory(Thing owner, List<Item> * output_sorted_list);
void find_items_on_floor(Coord location, List<Item> * output_sorted_list);

bool confuse_individual(Thing target);
void strike_individual(Thing attacker, Thing target);

void change_map(Coord location, TileType new_tile_type);

#endif
