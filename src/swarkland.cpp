#include "swarkland.hpp"

#include "path_finding.hpp"
#include "decision.hpp"
#include "display.hpp"
#include "event.hpp"
#include "item.hpp"

Species specieses[SpeciesId_COUNT];
IdMap<Thing> actual_things;

Thing you;
bool youre_still_alive = true;
long long time_counter = 0;

bool cheatcode_full_visibility;

static void init_specieses() {
    specieses[SpeciesId_HUMAN] = {SpeciesId_HUMAN, 12, 10, 3, {1, 0}, true};
    specieses[SpeciesId_OGRE] = {SpeciesId_OGRE, 24, 10, 2, {1, 0}, true};
    specieses[SpeciesId_DOG] = {SpeciesId_DOG, 12, 4, 2, {1, 0}, true};
    specieses[SpeciesId_PINK_BLOB] = {SpeciesId_PINK_BLOB, 48, 12, 4, {0, 1}, false};
    specieses[SpeciesId_AIR_ELEMENTAL] = {SpeciesId_AIR_ELEMENTAL, 6, 6, 1, {0, 1}, false};
}

static void pickup_item(Thing individual, Thing item, bool publish) {
    if (item->container_id != uint256::zero())
        panic("pickup item in someone's inventory");

    List<Thing> inventory;
    find_items_in_inventory(individual, &inventory);

    item->location = Coord::nowhere();
    item->container_id = individual->id;
    item->z_order = inventory.length();
    if (publish)
        publish_event(Event::individual_picks_up_item(individual, item));
}
void drop_item_to_the_floor(Thing item, Coord location) {
    List<Thing> items_on_floor;
    find_items_on_floor(location, &items_on_floor);
    item->location = location;
    item->container_id = uint256::zero();
    item->z_order = items_on_floor.length();
    publish_event(Event::item_drops_to_the_floor(item));
}

static const int no_spawn_radius = 10;

// specify SpeciesId_COUNT for random
Thing spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker) {
    while (species_id == SpeciesId_COUNT) {
        species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // humans are too hard. without giving one side a powerup, they're evenly matched.
            species_id = SpeciesId_COUNT;
        }
    }

    List<Coord> available_spawn_locations;
    for (Coord location = {0, 0}; location.y < map_size.y; location.y++) {
        for (location.x = 0; location.x < map_size.x; location.x++) {
            if (actual_map_tiles[location].tile_type == TileType_WALL)
                continue;
            if (you != NULL && distance_squared(location, you->location) < no_spawn_radius * no_spawn_radius)
                continue;
            if (find_individual_at(location) != NULL)
                continue;
            available_spawn_locations.append(location);
        }
    }
    if (available_spawn_locations.length() == 0) {
        // it must be pretty crowded in here
        return NULL;
    }
    Coord location = available_spawn_locations[random_int(available_spawn_locations.length())];

    Thing individual = create<ThingImpl>(species_id, location, team, decision_maker);

    if (random_int(1) == 0) {
        // have an item
        Thing item = random_item();
        pickup_item(individual, item, false);
    }

    actual_things.put(individual->id, individual);
    compute_vision(individual);
    publish_event(Event::appear(individual));
    return individual;
}

static void init_individuals() {
    you = spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_PLAYER);
    if (you == NULL)
        panic("can't spawn you");
    // have a friend
    spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_AI);

    // generate a few warm-up monsters
    for (int i = 0; i < 6; i++) {
        spawn_a_monster(SpeciesId_COUNT, Team_BAD_GUYS, DecisionMakerType_AI);
    }
}

void swarkland_init() {
    init_specieses();
    init_items();

    generate_map();

    init_individuals();
}

static void spawn_monsters(bool force_do_it) {
    if (force_do_it || random_int(120) == 0) {
        // spawn a monster
        if (random_int(50) == 0) {
            // a friend has arrived!
            spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_AI);
        } else {
            spawn_a_monster(SpeciesId_COUNT, Team_BAD_GUYS, DecisionMakerType_AI);
        }
    }
}

static void regen_hp(Thing individual) {
    if (individual->life()->hitpoints < individual->life()->species()->starting_hitpoints) {
        if (random_int(60) == 0) {
            individual->life()->hitpoints++;
        }
    }
}

static void kill_individual(Thing individual) {
    // drop your stuff
    List<Thing> inventory;
    find_items_in_inventory(individual, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        drop_item_to_the_floor(inventory[i], individual->location);

    individual->life()->hitpoints = 0;
    individual->still_exists = false;

    publish_event(Event::die(individual));

    if (individual == you)
        youre_still_alive = false;
    if (individual == cheatcode_spectator) {
        // our fun looking through the eyes of a dying man has ended. back to normal.
        cheatcode_spectator = NULL;
    }
}

static void damage_individual(Thing attacker, Thing target, int damage) {
    if (damage <= 0)
        panic("no damage");
    target->life()->hitpoints -= damage;
    if (target->life()->hitpoints <= 0) {
        kill_individual(target);
        attacker->life()->kill_counter++;
    }
}

// normal melee attack
static void attack(Thing attacker, Thing target) {
    publish_event(Event::attack(attacker, target));
    damage_individual(attacker, target, attacker->life()->species()->attack_power);
}

static int compare_things_by_z_order(Thing a, Thing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
static int compare_perceived_things_by_z_order(PerceivedThing a, PerceivedThing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
static int compare_perceived_things_by_type_and_z_order(PerceivedThing a, PerceivedThing b) {
    int result = a->thing_type - b->thing_type;
    if (result != 0)
        return result;
    return compare_perceived_things_by_z_order(a, b);
}

PerceivedThing find_perceived_individual_at(Thing observer, Coord location) {
    PerceivedThing individual;
    for (auto iterator = get_perceived_individuals(observer); iterator.next(&individual);)
        if (individual->location == location)
            return individual;
    return NULL;
}
void find_perceived_things_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing thing;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);)
        if (thing->location == location)
            output_sorted_list->append(thing);
    sort<PerceivedThing, compare_perceived_things_by_type_and_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
Thing find_individual_at(Coord location) {
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);) {
        if (!individual->still_exists)
            continue;
        if (individual->location.x == location.x && individual->location.y == location.y)
            return individual;
    }
    return NULL;
}

void find_items_in_inventory(Thing owner, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = actual_items(); iterator.next(&item);)
        if (item->container_id == owner->id)
            output_sorted_list->append(item);
    sort<Thing, compare_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void find_items_in_inventory(Thing observer, PerceivedThing perceived_owner, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing item;
    for (auto iterator = get_perceived_items(observer); iterator.next(&item);)
        if (item->container_id == perceived_owner->id)
            output_sorted_list->append(item);
    sort<PerceivedThing, compare_perceived_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void find_items_on_floor(Coord location, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = actual_items(); iterator.next(&item);)
        if (item->location == location)
            output_sorted_list->append(item);
    sort<Thing, compare_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}

static void do_move(Thing mover, Coord new_position) {
    Coord old_position = mover->location;
    mover->location = new_position;
    // gimme that
    List<Thing> floor_items;
    find_items_on_floor(new_position, &floor_items);
    for (int i = 0; i < floor_items.length(); i++)
        pickup_item(mover, floor_items[i], true);

    compute_vision(mover);

    // notify other individuals who could see that move
    publish_event(Event::move(mover, old_position, new_position));
}

static void cheatcode_kill_everybody_in_the_world() {
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);) {
        if (!individual->still_exists)
            continue;
        if (individual != you)
            kill_individual(individual);
    }
}
static void cheatcode_polymorph() {
    SpeciesId old_species = you->life()->species_id;
    you->life()->species_id = (SpeciesId)((old_species + 1) % SpeciesId_COUNT);
    compute_vision(you);
    publish_event(Event::polymorph(you, old_species));
}
Thing cheatcode_spectator;
void cheatcode_spectate() {
    Coord individual_at = get_mouse_tile(main_map_area);
    cheatcode_spectator = find_individual_at(individual_at);
}

static bool validate_action(Thing individual, Action action) {
    List<Action> valid_actions;
    get_available_actions(individual, valid_actions);
    for (int i = 0; i < valid_actions.length(); i++)
        if (valid_actions[i] == action)
            return true;
    return false;
}
static const Coord directions_by_rotation[] = {
    {+1,  0},
    {+1, +1},
    { 0, +1},
    {-1, +1},
    {-1,  0},
    {-1, -1},
    { 0, -1},
    {+1, -1},
};
Coord confuse_direction(Thing individual, Coord direction) {
    if (individual->status_effects.confused_timeout == 0)
        return direction; // not confused
    if (random_int(2) > 0)
        return direction; // you're ok this time
    // which direction are we attempting
    for (int i = 0; i < 8; i++) {
        if (directions_by_rotation[i] != direction)
            continue;
        // turn left or right 45 degrees
        i = euclidean_mod(i + 2 * random_int(2) - 1, 8);
        return directions_by_rotation[i];
    }
    panic("direection not found");
}

// return whether we did anything. also, cheatcodes take no time
static bool take_action(Thing actor, Action action) {
    bool is_valid = validate_action(actor, action);
    if (!is_valid) {
        if (actor == you) {
            // forgive the player for trying to run into a wall or something
            return false;
        }
        panic("ai tried to make an illegal move");
    }

    // we know you can attempt the action, but it won't necessarily turn out the way you expected it.
    actor->life()->movement_points = 0;

    switch (action.type) {
        case Action::WAIT:
            return true;
        case Action::UNDECIDED:
            panic("not a real action");
        case Action::MOVE: {
            // normally, we'd be sure that this was valid, but if you use cheatcodes,
            // monsters can try to walk into you while you're invisible.
            Coord new_position = actor->location + confuse_direction(actor, action.coord);
            if (!is_in_bounds(new_position) || actual_map_tiles[new_position].tile_type == TileType_WALL) {
                // this can only happen if your direction was changed, since attempting to move into a wall is invalid.
                publish_event(Event::bump_into_wall(actor, new_position));
                return true;
            }
            Thing target = find_individual_at(new_position);
            if (target != NULL) {
                // this is not attacking
                publish_event(Event::bump_into_individual(actor, target));
                return true;
            }
            // clear to move
            do_move(actor, new_position);
            return true;
        }
        case Action::ATTACK: {
            Coord new_position = actor->location + confuse_direction(actor, action.coord);
            Thing target = find_individual_at(new_position);
            if (target != NULL) {
                attack(actor, target);
                return true;
            } else {
                bool is_air = is_in_bounds(new_position) && actual_map_tiles[new_position].tile_type == TileType_FLOOR;
                if (is_air)
                    publish_event(Event::attack_thin_air(actor, new_position));
                else
                    publish_event(Event::attack_wall(actor, new_position));
                return true;
            }
        }
        case Action::ZAP: {
            zap_wand(actor, action.item, action.coord);
            return true;
        }
        case Action::DROP: {
            drop_item_to_the_floor(actual_things.get(action.item), actor->location);
            return true;
        }

        case Action::CHEATCODE_HEALTH_BOOST:
            actor->life()->hitpoints += 100;
            return false;
        case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
            cheatcode_kill_everybody_in_the_world();
            return false;
        case Action::CHEATCODE_POLYMORPH:
            cheatcode_polymorph();
            // this one does take time, because your movement cost may have changed
            return true;
        case Action::CHEATCODE_INVISIBILITY:
            if (actor->status_effects.invisible) {
                actor->status_effects.invisible = false;
                publish_event(Event::appear(you));
            } else {
                actor->status_effects.invisible = true;
                publish_event(Event::turn_invisible(you));
            }
            return false;
        case Action::CHEATCODE_GENERATE_MONSTER:
            spawn_monsters(true);
            return false;
    }
    panic("unimplemented action type");
}

List<Thing> poised_individuals;
int poised_individuals_index = 0;
// this function will return only when we're expecting player input
void run_the_game() {
    while (youre_still_alive) {
        if (poised_individuals.length() == 0) {
            time_counter++;

            spawn_monsters(false);

            List<Thing> dead_individuals;
            List<Event> deferred_events;
            // who's ready to make a move?
            Thing individual;
            for (auto iterator = actual_individuals(); iterator.next(&individual);) {
                if (!individual->still_exists) {
                    dead_individuals.append(individual);
                    continue;
                }
                // advance time for this individual
                regen_hp(individual);
                if (individual->status_effects.confused_timeout > 0) {
                    individual->status_effects.confused_timeout--;
                    if (individual->status_effects.confused_timeout == 0) {
                        deferred_events.append(Event::no_longer_confused(individual));
                    }
                }
                individual->life()->movement_points++;
                if (individual->life()->movement_points >= individual->life()->species()->movement_cost) {
                    poised_individuals.append(individual);
                    // log the passage of time in the message window.
                    // this actually only observers time in increments of your movement cost
                    if (individual->life()->species()->has_mind) {
                        List<RememberedEvent> & events = individual->life()->knowledge.remembered_events;
                        if (events.length() > 0 && events[events.length() - 1] != NULL)
                            events.append(NULL);
                    }
                }
            }

            // publish deferred events.
            // TODO: this exposes hashtable order
            for (int i = 0; i < deferred_events.length(); i++)
                publish_event(deferred_events[i]);

            // delete the dead
            for (int i = 0; i < dead_individuals.length(); i++)
                actual_things.remove(dead_individuals[i]->id);

            // who really gets to go first is determined by initiative
            sort<Thing, compare_individuals_by_initiative>(poised_individuals.raw(), poised_individuals.length());
        }

        // move individuals
        for (; poised_individuals_index < poised_individuals.length(); poised_individuals_index++) {
            Thing individual = poised_individuals[poised_individuals_index];
            if (!individual->still_exists)
                continue; // sorry, buddy. you were that close to making another move.
            Action action = decision_makers[individual->life()->decision_maker](individual);
            if (action == Action::undecided()) {
                // give the player some time to think.
                // we'll resume right back where we left off.
                return;
            }
            bool actually_did_anything = take_action(individual, action);
            if (!actually_did_anything) {
                // sigh... the player is derping around
                return;
            }
        }
        poised_individuals.clear();
        poised_individuals_index = 0;
    }
}

// wait will always be available
void get_available_actions(Thing individual, List<Action> & output_actions) {
    output_actions.append(Action::wait());
    // move
    for (int i = 0; i < 8; i++) {
        Coord direction = directions[i];
        if (do_i_think_i_can_move_here(individual, individual->location + direction))
            output_actions.append(Action::move(direction));
    }
    // attack
    PerceivedThing target;
    for (auto iterator = get_perceived_individuals(individual); iterator.next(&target);) {
        if (target->id == individual->id)
            continue; // you can't attack yourself, sorry.
        Coord vector = target->location - individual->location;
        if (vector == sign(vector)) {
            // within melee range
            output_actions.append(Action::attack(vector));
        }
    }
    // use items
    List<Thing> inventory;
    find_items_in_inventory(individual, &inventory);
    for (int i = 0; i < inventory.length(); i++) {
        for (int j = 0; j < 8; j++)
            output_actions.append(Action::zap(inventory[i]->id, directions[j]));
        output_actions.append(Action::drop_item(inventory[i]->id));
    }

    // alright, we'll let you use cheatcodes
    if (individual == you) {
        output_actions.append(Action::cheatcode_health_boost());
        output_actions.append(Action::cheatcode_kill_everybody_in_the_world());
        output_actions.append(Action::cheatcode_polymorph());
        output_actions.append(Action::cheatcode_invisibility());
        output_actions.append(Action::cheatcode_generate_monster());
    }
}

// you need to emit events yourself
bool confuse_individual(Thing target) {
    if (!target->life()->species()->has_mind) {
        // can't confuse something with no mind
        return false;
    }
    target->status_effects.confused_timeout = random_int(100, 200);
    return true;
}

void strike_individual(Thing attacker, Thing target) {
    // it's just some damage
    int damage = random_int(4, 8);
    damage_individual(attacker, target, damage);
}
void change_map(Coord location, TileType new_tile_type) {
    actual_map_tiles[location].tile_type = new_tile_type;
    // recompute everyone's vision
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);)
        compute_vision(individual);
}
