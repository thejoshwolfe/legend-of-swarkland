#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "thing.hpp"
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
    PerceivedThing container = observer->life()->knowledge.perceived_things.get(target->container_id, nullptr);
    if (container == nullptr) {
        // don't know where the owner went.
        // TODO: this is always a bug. this hack just puts off finding the true cause.
        fprintf(stderr, "warning: phantom item bug\n");
        return Coord::nowhere();
    }
    return get_thing_location(observer, container);
}

void swarkland_init();

void get_available_moves(List<Action> * output_actions);
void get_available_actions(Thing individual, List<Action> * output_actions);
bool can_move(Thing actor);
bool can_act(Thing actor);
static inline int get_movement_cost(Thing actor) {
    if (actor->status_effects.speed_up_expiration_time > time_counter)
        return 3;
    return actor->life()->species()->movement_cost;
}

void run_the_game();
int compare_perceived_things_by_type_and_z_order(PerceivedThing a, PerceivedThing b);
PerceivedThing find_perceived_individual_at(Thing observer, Coord location);
void find_perceived_things_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
void find_perceived_items_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
Thing find_individual_at(Coord location);
void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list);
void find_items_in_inventory(Thing observer, PerceivedThing perceived_owner, List<PerceivedThing> * output_sorted_list);
void find_items_on_floor(Coord location, List<Thing> * output_sorted_list);
void drop_item_to_the_floor(Thing item, Coord location);

void confuse_individual(Thing target);
void speed_up_individual(Thing target);
void strike_individual(Thing attacker, Thing target);
void poison_individual(Thing attacker, Thing target);
void heal_hp(Thing individual, int hp);

void change_map(Coord location, TileType new_tile_type);

void fix_z_orders(uint256 container_id);
void fix_z_orders(Coord location);

#endif
