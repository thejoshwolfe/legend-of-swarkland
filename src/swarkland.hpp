#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "thing.hpp"
#include "action.hpp"

extern Species specieses[SpeciesId_COUNT];
extern Mind specieses_mind[SpeciesId_COUNT];

extern bool test_mode;

extern IdMap<Thing> actual_things;
// the good guy
extern Thing you;
extern bool youre_still_alive;
extern int64_t time_counter;
// usually you, but cheatcodes can allow you to control several monsters at once.
extern Thing player_actor;

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
    PerceivedThing container = observer->life()->knowledge.perceived_things.get(target->container_id);
    return get_thing_location(observer, container);
}
static inline bool individual_has_mind(Thing thing) {
    switch (specieses_mind[thing->life()->species_id]) {
        case Mind_NONE:
            return false;
        case Mind_INSTINCT:
        case Mind_SAPIENT_DERPER:
        case Mind_SAPIENT_CLEVER:
            return true;
    }
    unreachable();
}
static inline bool individual_uses_items(Thing thing) {
    switch (specieses_mind[thing->life()->species_id]) {
        case Mind_NONE:
        case Mind_INSTINCT:
            return false;
        case Mind_SAPIENT_DERPER:
        case Mind_SAPIENT_CLEVER:
            return true;
    }
    unreachable();
}
static inline bool individual_is_clever(Thing thing) {
    switch (specieses_mind[thing->life()->species_id]) {
        case Mind_NONE:
        case Mind_INSTINCT:
        case Mind_SAPIENT_DERPER:
            return false;
        case Mind_SAPIENT_CLEVER:
            return true;
    }
    unreachable();
}

void swarkland_init();

bool validate_action(Thing actor, Action action);
bool can_move(Thing actor);
static inline int get_movement_cost(Thing actor) {
    if (has_status(actor, StatusEffect::SPEED))
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

void fix_perceived_z_orders(Thing observer, uint256 container_id);
void fix_z_orders(uint256 container_id);
void fix_z_orders(Coord location);

#endif
