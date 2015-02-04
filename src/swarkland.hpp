#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

struct Action {
    enum Type {
        MOVE,
        WAIT,
        ATTACK,
        UNDECIDED, // TODO: smells bad
    };
    Type type;
    Coord coord;
};

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Individual> individuals;
extern Individual you;
extern long long time_counter;

extern bool cheatcode_full_visibility;
void cheatcode_kill_everybody_in_the_world();
void cheatcode_polymorph();
extern Individual cheatcode_spectator;
void cheatcode_spectate(Coord individual_at);

void swarkland_init();

void get_available_actions(Individual individual, List<Action> & output_actions);

Individual spawn_a_monster(SpeciesId species_id);
void advance_time();
void take_action(Individual individual, Action action);
PerceivedIndividual find_perceived_individual_at(Individual observer, Coord location);
Individual find_individual_at(Coord location);

#endif
