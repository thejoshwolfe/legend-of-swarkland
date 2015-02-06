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

        CHEATCODE_HEALTH_BOOST,
        CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD,
        CHEATCODE_POLYMORPH,
        CHEATCODE_INVISIBILITY,
        CHEATCODE_GENERATE_MONSTER,

        // only a player can be undecided
        UNDECIDED,
    };
    Type type;
    Coord coord;

    // canonical singletons, appropriate for == comparison
    static inline Action wait() {
        return {WAIT, {0, 0}};
    }
    static inline Action undecided() {
        return {UNDECIDED, {0, 0}};
    }

    static inline Action cheatcode_health_boost() {
        return {CHEATCODE_HEALTH_BOOST, {0, 0}};
    }
    static inline Action cheatcode_kill_everybody_in_the_world() {
        return {CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD, {0, 0}};
    }
    static inline Action cheatcode_polymorph() {
        return {CHEATCODE_POLYMORPH, {0, 0}};
    }
    static inline Action cheatcode_invisibility() {
        return {CHEATCODE_INVISIBILITY, {0, 0}};
    }
    static inline Action cheatcode_generate_monster() {
        return {CHEATCODE_GENERATE_MONSTER, {0, 0}};
    }
};
static inline bool operator==(Action a, Action b) {
    return a.type == b.type && a.coord == b.coord;
}
static inline bool operator!=(Action a, Action b) {
    return !(a == b);
}

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Individual> actual_individuals;
extern Individual you;
extern bool youre_still_alive;
extern long long time_counter;

extern bool cheatcode_full_visibility;
extern Individual cheatcode_spectator;
void cheatcode_spectate(Coord individual_at);

void swarkland_init();

void get_available_actions(Individual individual, List<Action> & output_actions);

Individual spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker);
void run_the_game();
PerceivedIndividual find_perceived_individual_at(Individual observer, Coord location);
Individual find_individual_at(Coord location);

#endif
