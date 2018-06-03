#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "hashtable.hpp"
#include "geometry.hpp"
#include "list.hpp"
#include "random.hpp"

#include "thing.hpp"
#include "item.hpp"
#include "action.hpp"

extern bool print_diagnostics;
extern int snapshot_interval;
extern bool headless_mode;

struct Game {
    WandDescriptionId actual_wand_descriptions[WandId_COUNT];
    PotionDescriptionId actual_potion_descriptions[PotionId_COUNT];
    BookDescriptionId actual_book_descriptions[BookId_COUNT];
    uint256 you_id = uint256::zero();
    uint256 player_actor_id;

    int64_t time_counter = 0;

    // starts at 1
    int dungeon_level = 0;
    MapMatrix<TileType> actual_map_tiles;
    MapMatrix<uint8_t> aesthetic_indexes;

    bool test_mode;
    // for test mode
    uint256 random_arbitrary_large_number_count = uint256::zero();
    uint256 random_initiative_count = uint256::zero();
    // for normal mode
    RandomState the_random_state;
    int item_pool[TOTAL_ITEMS];

    IdMap<Thing> actual_things;
    IdMap<uint256> observer_to_active_identifiable_item;
};

extern bool cheatcode_full_visibility;
extern Thing cheatcode_spectator;

extern Game * game;

static inline Thing you()          { return game->actual_things.get(game->you_id); }
static inline Thing player_actor() { return game->actual_things.get(game->player_actor_id); }

static inline uint256 random_id() {
    if (game->test_mode) {
        // just increment a counter
        game->random_arbitrary_large_number_count.values[3]++;
        return game->random_arbitrary_large_number_count;
    }
    return random_uint256();
}

void cheatcode_spectate(Coord location);

static inline bool is_actual_individual(Thing thing) {
    return thing->still_exists && thing->thing_type == ThingType_INDIVIDUAL;
}
static inline FilteredIterator<IdMap<Thing>::Iterator, Thing> actual_individuals() {
    return FilteredIterator<IdMap<Thing>::Iterator, Thing>(game->actual_things.value_iterator(), is_actual_individual);
}
static inline Coord get_location_coord(Thing thing) {
    while (true) {
        switch (thing->location.type) {
            case Location::NOWHERE:
                unreachable();
            case Location::STANDING:
                return thing->location.standing();
            case Location::FLOOR_PILE:
                return thing->location.floor_pile().coord;
            case Location::INVENTORY:
                thing = game->actual_things.get(thing->location.inventory().container_id);
                continue;
        }
        unreachable();
    }
}
static inline Coord get_standing_or_floor_coord(Location location) {
    switch (location.type) {
        case Location::NOWHERE:
        case Location::INVENTORY:
            unreachable();
        case Location::STANDING:
            return location.standing();
        case Location::FLOOR_PILE:
            return location.floor_pile().coord;
    }
    unreachable();
}
static inline bool is_standing_or_floor(Location location) {
    switch (location.type) {
        case Location::NOWHERE:
        case Location::INVENTORY:
            return false;
        case Location::STANDING:
        case Location::FLOOR_PILE:
            return true;
    }
    unreachable();
}
static inline Nullable<Coord> get_maybe_standing_or_floor_coord(Location location) {
    switch (location.type) {
        case Location::NOWHERE:
        case Location::INVENTORY:
            return nullptr;
        case Location::STANDING:
            return location.standing();
        case Location::FLOOR_PILE:
            return location.floor_pile().coord;
    }
    unreachable();
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
    if (thing->location.type == Location::INVENTORY)
        thing = observer->life()->knowledge.perceived_things.get(thing->location.inventory().container_id);
    return has_status_apparently(thing, StatusEffect::INVISIBILITY);
}
static inline bool is_invisible(Thing thing) {
    if (thing->location.type == Location::INVENTORY)
        thing = game->actual_things.get(thing->location.inventory().container_id);
    return has_status_effectively(thing, StatusEffect::INVISIBILITY);
}

void swarkland_init();

bool validate_action(Thing actor, const Action & action);
bool can_move(Thing actor);
bool is_touching_ground(Thing individual);
int get_movement_cost(Thing actor);

void run_the_game();
int compare_perceived_things_canonically(const PerceivedThing & a, const PerceivedThing & b);
static inline int compare_floor_items_by_z_order(const PerceivedThing & a, const PerceivedThing & b) {
    return compare(a->location.floor_pile().z_order, b->location.floor_pile().z_order);
}
PerceivedThing find_perceived_individual_at(Thing observer, Coord location);
void find_perceived_things_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
void find_perceived_items_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list);
Thing find_individual_at(Coord location);
Thing get_equipped_weapon(Thing individual);
void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list);
void find_items_in_inventory(Thing observer, uint256 container_id, List<PerceivedThing> * output_sorted_list);
void find_items_on_floor(Coord location, List<Thing> * output_sorted_list);
void drop_item_to_the_floor(Thing item, Coord location);
void get_abilities(Thing individual, List<AbilityId> * output_sorted_abilities);
bool is_ability_ready(Thing actor, AbilityId ability_id);
void suck_up_item(Thing actor, Thing item);
bool attempt_move(Thing actor, Coord new_position, Thing who_is_responsible);
bool attempt_dodge(Thing attacker, Thing actor);
void apply_impulse(Thing individual, Coord vector);

bool check_for_status_expired(Thing individual, int index);
void polymorph_individual(Thing individual, SpeciesId species_id, int duration);
void damage_individual(Thing target, int damage, Thing attacker, bool is_melee);
void poison_individual(Thing attacker, Thing target);
void blind_individual(Thing attacker, Thing target, int expiration_midpoint);
void slow_individual(Thing attacker, Thing target);
void heal_hp(Thing individual, int hp);
void use_mana(Thing actor, int mana);
void gain_mp(Thing individual, int mp);

void change_map(Coord location, TileType new_tile_type);

void set_location(Thing thing, Location location);
void set_location(Thing observer, PerceivedThing thing, Location location);

#endif
