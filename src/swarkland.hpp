#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "thing.hpp"
#include "action.hpp"

extern bool test_mode;
extern bool headless_mode;

extern IdMap<Thing> actual_things;
// the good guy
extern Thing you;
extern bool youre_still_alive;
extern int64_t time_counter;
// usually just you, but cheatcodes can allow you to control several monsters in rotation.
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
static inline Thing get_top_level_container(Thing thing) {
    while (thing->container_id != uint256::zero())
        thing = actual_things.get(thing->container_id);
    return thing;
}
static inline PerceivedThing get_top_level_container(const Thing & observer, PerceivedThing thing) {
    while (thing->container_id != uint256::zero())
        thing = observer->life()->knowledge.perceived_things.get(thing->container_id);
    return thing;
}
static inline bool individual_uses_items(Thing thing) {
    switch (thing->mental_species()->mind) {
        case Mind_NONE:
        case Mind_BEAST:
            return false;
        case Mind_SAVAGE:
        case Mind_CIVILIZED:
            return true;
    }
    unreachable();
}
static inline bool individual_is_clever(Thing thing) {
    switch (thing->mental_species()->mind) {
        case Mind_NONE:
        case Mind_BEAST:
        case Mind_SAVAGE:
            return false;
        case Mind_CIVILIZED:
            return true;
    }
    unreachable();
}

static inline bool is_invisible(Thing observer, PerceivedThing thing) {
    return has_status(get_top_level_container(observer, thing), StatusEffect::INVISIBILITY);
}

void swarkland_init();

bool validate_action(Thing actor, const Action & action);
bool can_move(Thing actor);
static inline int get_movement_cost(Thing actor) {
    if (has_status(actor, StatusEffect::SPEED))
        return 3;
    return actor->physical_species()->movement_cost;
}

void run_the_game();
int compare_perceived_things_by_type_and_z_order(PerceivedThing a, PerceivedThing b);
PerceivedThing find_perceived_individual_at(Thing observer, Coord location);
void find_perceived_things_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
void find_perceived_items_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
Thing find_individual_at(Coord location);
void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list);
void find_items_in_inventory(Thing observer, uint256 container_id, List<PerceivedThing> * output_sorted_list);
void find_items_on_floor(Coord location, List<Thing> * output_sorted_list);
void drop_item_to_the_floor(Thing item, Coord location);
void get_abilities(Thing individual, List<Ability::Id> * output_sorted_abilities);
bool is_ability_ready(Thing actor, Ability::Id ability_id);
void attempt_move(Thing actor, Coord new_position);

bool check_for_status_expired(Thing individual, int index);
void polymorph_individual(Thing individual, SpeciesId species_id);
void damage_individual(Thing target, int damage, Thing attacker, bool is_melee);
void poison_individual(Thing attacker, Thing target);
void heal_hp(Thing individual, int hp);
void use_mana(Thing actor, int mana);
void gain_mp(Thing individual, int mp);

void change_map(Coord location, TileType new_tile_type);

void fix_perceived_z_orders(Thing observer, uint256 container_id);
void fix_z_orders(uint256 container_id);
void fix_z_orders(Coord location);

#endif
