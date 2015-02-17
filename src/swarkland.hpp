#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"
#include "action.hpp"

extern Species specieses[SpeciesId_COUNT];

extern IdMap<Thing> actual_things;
extern Thing you;
extern bool youre_still_alive;
extern long long time_counter;

extern bool cheatcode_full_visibility;
extern Thing cheatcode_spectator;
void cheatcode_spectate();

static inline FilteredIterator<IdMap<Thing>::Iterator, Thing> actual_individuals() {
    return FilteredIterator<IdMap<Thing>::Iterator, Thing>(actual_things.value_iterator(), is_individual);
}
static inline FilteredIterator<IdMap<Thing>::Iterator, Thing> actual_items() {
    return FilteredIterator<IdMap<Thing>::Iterator, Thing>(actual_things.value_iterator(), is_item);
}
static inline Coord get_thing_location(Thing observer, const PerceivedThing & target) {
    if (target->location != Coord::nowhere())
        return target->location;
    return get_thing_location(observer, observer->life()->knowledge.perceived_things.get(target->container_id));
}

void swarkland_init();

void get_available_actions(Thing individual, List<Action> & output_actions);

Thing spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker);
void run_the_game();
PerceivedThing find_perceived_individual_at(Thing observer, Coord location);
Thing find_individual_at(Coord location);
void find_items_in_inventory(Thing owner, List<Thing> * output_sorted_list);
void find_items_in_inventory(Thing observer, PerceivedThing perceived_owner, List<PerceivedThing> * output_sorted_list);
void find_items_on_floor(Coord location, List<Thing> * output_sorted_list);

bool confuse_individual(Thing target);
void strike_individual(Thing attacker, Thing target);

void change_map(Coord location, TileType new_tile_type);

#endif
