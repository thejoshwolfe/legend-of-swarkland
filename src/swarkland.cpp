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
    specieses[SpeciesId_HUMAN] = {SpeciesId_HUMAN, 12, 10, 3, {1, 0}, true, false, false, true};
    specieses[SpeciesId_OGRE] = {SpeciesId_OGRE, 24, 10, 2, {1, 0}, true, false, false, true};
    specieses[SpeciesId_DOG] = {SpeciesId_DOG, 12, 4, 2, {1, 0}, true, false, false, false};
    specieses[SpeciesId_PINK_BLOB] = {SpeciesId_PINK_BLOB, 48, 12, 4, {0, 1}, false, true, false, false};
    specieses[SpeciesId_AIR_ELEMENTAL] = {SpeciesId_AIR_ELEMENTAL, 6, 6, 1, {0, 1}, false, true, true, false};
}

static void kill_individual(Thing individual) {
    individual->life()->hitpoints = 0;
    individual->still_exists = false;

    publish_event(Event::die(individual));

    // drop your stuff
    List<Thing> inventory;
    find_items_in_inventory(individual->id, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        drop_item_to_the_floor(inventory[i], individual->location);

    if (individual == you)
        youre_still_alive = false;
    if (individual == cheatcode_spectator) {
        // our fun looking through the eyes of a dying man has ended. back to normal.
        cheatcode_spectator = NULL;
    }
}

static void level_up(Thing individual, bool publish) {
    Life * life = individual->life();
    int old_max_hitpoints = life->max_hitpoints();
    life->experience = life->next_level_up();
    int new_max_hitpoints = life->max_hitpoints();
    life->hitpoints = life->hitpoints * new_max_hitpoints / old_max_hitpoints;
    if (publish)
        publish_event(Event::level_up(individual->id));
}

static void gain_experience(Thing individual, int delta, bool publish) {
    Life * life = individual->life();
    int new_expderience = life->experience + delta;
    while (new_expderience >= life->next_level_up())
        level_up(individual, publish);
    life->experience = new_expderience;
}

static void reset_hp_regen_timeout(Thing individual) {
    Life * life = individual->life();
    if (life->hitpoints < life->max_hitpoints())
        life->hp_regen_deadline = time_counter + 12 * random_inclusive(5, 9);
}
static void damage_individual(Thing attacker, Thing target, int damage) {
    if (damage <= 0)
        panic("no damage");
    target->life()->hitpoints -= damage;
    reset_hp_regen_timeout(target);
    if (target->life()->hitpoints <= 0) {
        kill_individual(target);
        gain_experience(attacker, 1, true);
    }
}

// publish the event yourself
static void pickup_item(Thing individual, Thing item) {
    if (item->container_id != uint256::zero())
        panic("pickup item in someone's inventory");

    Coord old_location = item->location;
    item->location = Coord::nowhere();
    fix_z_orders(old_location);

    // put it at the end of the inventory
    item->container_id = individual->id;
    item->z_order = 0x7fffffff;
    fix_z_orders(individual->id);
}
void drop_item_to_the_floor(Thing item, Coord location) {
    uint256 old_container_id = item->container_id;

    List<Thing> items_on_floor;
    find_items_on_floor(location, &items_on_floor);
    item->container_id = uint256::zero();
    fix_z_orders(old_container_id);

    // put it on the top of pile
    item->location = location;
    item->z_order = -0x80000000;
    fix_z_orders(location);

    publish_event(Event::item_drops_to_the_floor(item));

    Thing individual = find_individual_at(location);
    if (individual != NULL && individual->life()->species()->sucks_up_items) {
        // suck it
        pickup_item(individual, item);
        publish_event(Event::individual_sucks_up_item(individual, item));
    }
}
static void throw_item(Thing actor, Thing item, Coord direction) {
    publish_event(Event::throw_item(actor->id, item->id));
    // find the hit target
    int range = random_int(3, 6);
    Coord cursor = actor->location;
    Coord air_explode_center = Coord::nowhere();
    Coord wall_explode_center = Coord::nowhere();
    for (int i = 0; i < range; i++) {
        cursor += direction;
        if (is_in_bounds(cursor)) {
            Thing target = find_individual_at(cursor);
            if (target != NULL) {
                publish_event(Event::item_hits_individual(item->id, target->id));
                // hurt a little
                int damage = random_int(1, 3);
                damage_individual(actor, target, damage);
                if (damage == 2)
                    air_explode_center = wall_explode_center = cursor;
                break;
            }
        }
        if (!is_in_bounds(cursor) || !is_open_space(actual_map_tiles[cursor].tile_type)) {
            // TODO: remove this hack once the edge of the world is less reachable.
            Coord wall_location = clamp(cursor, {0, 0}, map_size);
            publish_event(Event::item_hits_wall(item->id, wall_location));
            if (random_int(2) == 0) {
                wall_explode_center = wall_location;
                air_explode_center = cursor - direction;
            } else {
                // back up one and drop it
                cursor -= direction;
            }
            break;
        }
    }
    if (air_explode_center != Coord::nowhere()) {
        // boom
        WandId wand_id = actual_wand_identities[item->wand_info()->description_id];
        Coord center;
        int apothem;
        if (wand_id == WandId_WAND_OF_DIGGING) {
            center = wall_explode_center;
            apothem = 2; // 5x5
        } else {
            center = air_explode_center;
            apothem = 1; // 3x3
        }
        IdMap<WandDescriptionId> perceived_current_zapper;
        publish_event(Event::wand_explodes(item->id, center), &perceived_current_zapper);
        actual_things.remove(item->id);
        fix_z_orders(actor->id);

        List<Thing> affected_individuals;
        Thing individual;
        for (auto iterator = actual_individuals(); iterator.next(&individual);) {
            if (!individual->still_exists)
                continue;
            Coord abs_vector = abs(individual->location - center);
            if (max(abs_vector.x, abs_vector.y) <= apothem)
                affected_individuals.append(individual);
        }
        sort<Thing, compare_things_by_id>(affected_individuals.raw(), affected_individuals.length());

        List<Coord> affected_walls;
        Coord upper_left  = clamp(center - Coord{apothem, apothem}, {0, 0}, map_size - Coord{1, 1});
        Coord lower_right = clamp(center + Coord{apothem, apothem}, {0, 0}, map_size - Coord{1, 1});
        for (Coord wall_cursor = upper_left; wall_cursor.y <= lower_right.y; wall_cursor.y++)
            for (wall_cursor.x = upper_left.x; wall_cursor.x <= lower_right.x; wall_cursor.x++)
                if (actual_map_tiles[wall_cursor].tile_type == TileType_WALL)
                    affected_walls.append(wall_cursor);

        switch (wand_id) {
            case WandId_WAND_OF_CONFUSION:
                for (int i = 0; i < affected_individuals.length(); i++) {
                    Thing target = affected_individuals[i];
                    if (confuse_individual(target)) {
                        publish_event(Event::explosion_of_confusion_hit_individual(target), &perceived_current_zapper);
                    } else {
                        publish_event(Event::explosion_hit_individual_no_effect(target), &perceived_current_zapper);
                    }
                }
                for (int i = 0; i < affected_walls.length(); i++)
                    publish_event(Event::explosion_hit_wall_no_effect(affected_walls[i]), &perceived_current_zapper);
                break;
            case WandId_WAND_OF_DIGGING:
                for (int i = 0; i < affected_individuals.length(); i++)
                    publish_event(Event::explosion_hit_individual_no_effect(affected_individuals[i]), &perceived_current_zapper);
                for (int i = 0; i < affected_walls.length(); i++) {
                    Coord wall_location = affected_walls[i];
                    change_map(wall_location, TileType_FLOOR);
                    publish_event(Event::beam_of_digging_hit_wall(wall_location), &perceived_current_zapper);
                }
                break;
            case WandId_WAND_OF_STRIKING:
                for (int i = 0; i < affected_individuals.length(); i++) {
                    Thing target = affected_individuals[i];
                    strike_individual(actor, target);
                    publish_event(Event::explosion_of_striking_hit_individual(target), &perceived_current_zapper);
                }
                for (int i = 0; i < affected_walls.length(); i++)
                    publish_event(Event::explosion_hit_wall_no_effect(affected_walls[i]), &perceived_current_zapper);
                break;

            case WandId_COUNT:
            case WandId_UNKNOWN:
                panic("not a real wand id");
        }

    } else {
        drop_item_to_the_floor(item, cursor);
    }
}


static const int no_spawn_radius = 10;
Coord find_random_location(Coord away_from_location) {
    List<Coord> available_spawn_locations;
    for (Coord location = {0, 0}; location.y < map_size.y; location.y++) {
        for (location.x = 0; location.x < map_size.x; location.x++) {
            if (!is_open_space(actual_map_tiles[location].tile_type))
                continue;
            if (euclidean_distance_squared(location, away_from_location) < no_spawn_radius * no_spawn_radius)
                continue;
            if (find_individual_at(location) != NULL)
                continue;
            available_spawn_locations.append(location);
        }
    }
    if (available_spawn_locations.length() == 0)
        return Coord::nowhere();
    return available_spawn_locations[random_int(available_spawn_locations.length())];
}

// SpeciesId_COUNT => random
// experience = -1 => random
static Thing spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker, int experience) {
    while (species_id == SpeciesId_COUNT) {
        species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // humans are too hard. without giving one side a powerup, they're evenly matched.
            species_id = SpeciesId_COUNT;
        }
    }

    // don't spawn monsters near you. don't spawn you near the stairs.
    Coord away_from_location = you != NULL ? you->location : stairs_down_location;
    Coord location = find_random_location(away_from_location);
    if (location == Coord::nowhere()) {
        // it must be pretty crowded in here
        return NULL;
    }

    Thing individual = create<ThingImpl>(species_id, location, team, decision_maker);

    if (experience == -1) {
        // monster experience scales around
        int midpoint = (you->life()->experience + level_to_experience(dungeon_level)) / 2;
        experience = random_inclusive(midpoint / 2, midpoint * 3 / 2);
    }
    gain_experience(individual, experience, false);

    if (random_int(10) == 0) {
        // have an item
        Thing item = random_item();
        pickup_item(individual, item);
    }

    actual_things.put(individual->id, individual);
    compute_vision(individual);
    publish_event(Event::appear(individual));
    return individual;
}

static void init_individuals() {
    if (you == NULL) {
        you = spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_PLAYER, 0);
    } else {
        // you just landed from upstairs
        // make sure the up and down stairs are sufficiently far appart.
        you->location = find_random_location(stairs_down_location);
        compute_vision(you);
    }
    if (random_int(dungeon_level) == 0) {
        // have a friend
        spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_AI, -1);
    }

    // generate a few warm-up monsters
    for (int i = 0; i < 6; i++)
        spawn_a_monster(SpeciesId_COUNT, Team_BAD_GUYS, DecisionMakerType_AI, -1);
}

void swarkland_init() {
    init_specieses();
    init_items();

    generate_map();

    init_individuals();
}

void go_down() {
    // goodbye everyone
    Thing thing;
    for (auto iterator = actual_things.value_iterator(); iterator.next(&thing);) {
        if (thing == you)
            continue; // you're cool
        if (thing->thing_type == ThingType_WAND)
            if (thing->container_id == you->id)
                continue; // take it with you
        // leave this behind us
        thing->still_exists = false;
    }

    you->life()->knowledge.reset_map();
    you->life()->knowledge.perceived_things.clear();
    generate_map();
    init_individuals();
}

static void spawn_random_individual() {
    if (random_int(50) == 0) {
        // a friend has arrived!
        spawn_a_monster(SpeciesId_HUMAN, Team_GOOD_GUYS, DecisionMakerType_AI, you->life()->experience);
    } else {
        spawn_a_monster(SpeciesId_COUNT, Team_BAD_GUYS, DecisionMakerType_AI, -1);
    }
}
static void maybe_spawn_monsters() {
    // asymptotically approach 1 monster per human decision.
    int numerator = dungeon_level;
    int denominator = 12 * dungeon_level + 600;
    if (random_int(denominator) <= numerator)
        spawn_random_individual();
}

static void create_item(Coord floor_location) {
    Thing item = random_item();
    drop_item_to_the_floor(item, floor_location);
}

static void regen_hp(Thing individual) {
    Life * life = individual->life();
    if (life->hp_regen_deadline == time_counter) {
        int hp_heal = random_inclusive(1, max(1, life->max_hitpoints() / 5));
        life->hitpoints = min(life->hitpoints + hp_heal, life->max_hitpoints());
        reset_hp_regen_timeout(individual);
    }
}

// normal melee attack
static void attack(Thing attacker, Thing target) {
    publish_event(Event::attack(attacker, target));
    int attack_power = attacker->life()->attack_power();
    int damage = (attack_power + 1) / 2 + random_inclusive(0, attack_power / 2);
    damage_individual(attacker, target, damage);
    reset_hp_regen_timeout(attacker);
}

static int compare_things_by_z_order(Thing a, Thing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
static int compare_perceived_things_by_z_order(PerceivedThing a, PerceivedThing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
int compare_perceived_things_by_type_and_z_order(PerceivedThing a, PerceivedThing b) {
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

void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = actual_items(); iterator.next(&item);)
        if (item->container_id == container_id)
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

    compute_vision(mover);

    // notify other individuals who could see that move
    publish_event(Event::move(mover, old_position, new_position));

    if (mover->life()->species()->sucks_up_items) {
        // pick up items for free
        List<Thing> floor_items;
        find_items_on_floor(new_position, &floor_items);
        for (int i = 0; i < floor_items.length(); i++) {
            pickup_item(mover, floor_items[i]);
            publish_event(Event::individual_sucks_up_item(mover, floor_items[i]));
        }
    }
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
            if (!is_open_space(actual_map_tiles[new_position].tile_type)) {
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
                bool is_air = is_open_space(actual_map_tiles[new_position].tile_type);
                if (is_air)
                    publish_event(Event::attack_thin_air(actor, new_position));
                else
                    publish_event(Event::attack_wall(actor, new_position));
                return true;
            }
        }
        case Action::ZAP:
            zap_wand(actor, action.item, action.coord);
            return true;
        case Action::PICKUP:
            pickup_item(actor, actual_things.get(action.item));
            publish_event(Event::individual_picks_up_item(actor, actual_things.get(action.item)));
            return true;
        case Action::DROP:
            drop_item_to_the_floor(actual_things.get(action.item), actor->location);
            return true;
        case Action::THROW:
            throw_item(actor, actual_things.get(action.item), action.coord);
            return true;

        case Action::GO_DOWN:
            go_down();
            return true;

        case Action::CHEATCODE_HEALTH_BOOST:
            actor->life()->hitpoints += 100;
            actor->life()->hp_regen_deadline = time_counter - 1;
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
            spawn_random_individual();
            return false;
        case Action::CHEATCODE_CREATE_ITEM:
            create_item(you->location);
            return false;
        case Action::CHEATCODE_GO_DOWN:
            go_down();
            return true;
        case Action::CHEATCODE_GAIN_LEVEL:
            gain_experience(actor, 10, true);
            return false;
    }
    panic("unimplemented action type");
}

// advance time for an individual
static void age_individual(Thing individual) {
    regen_hp(individual);

    if (individual->life()->species()->auto_throws_items) {
        // there's a chance per item they're carrying
        List<Thing> inventory;
        find_items_in_inventory(individual->id, &inventory);
        for (int i = 0; i < inventory.length(); i++) {
            if (random_int(100) == 0) {
                // throw item in random direction
                throw_item(individual, inventory[i], directions[random_int(8)]);
            }
        }
    }

    if (individual->status_effects.confused_timeout > 0) {
        individual->status_effects.confused_timeout--;
        if (individual->status_effects.confused_timeout == 0)
            publish_event(Event::no_longer_confused(individual));
    }

    List<RememberedEvent> & remembered_events = individual->life()->knowledge.remembered_events;
    if (remembered_events.length() >= 1000) {
        remembered_events.remove_range(0, 500);
        individual->life()->knowledge.event_forget_counter++;
    }

    individual->life()->movement_points++;
}

List<Thing> poised_individuals;
int poised_individuals_index = 0;
// this function will return only when we're expecting player input
void run_the_game() {
    while (youre_still_alive) {
        if (poised_individuals.length() == 0) {
            time_counter++;

            maybe_spawn_monsters();

            // who's ready to make a move?
            List<Thing> turn_order;
            {
                Thing individual;
                for (auto iterator = actual_individuals(); iterator.next(&individual);)
                    turn_order.append(individual);
            }
            sort<Thing, compare_individuals_by_initiative>(turn_order.raw(), turn_order.length());

            for (int i = 0; i < turn_order.length(); i++) {
                Thing individual = turn_order[i];
                if (!individual->still_exists) {
                    actual_things.remove(individual->id);
                    continue;
                }
                age_individual(individual);
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
    // pickup items
    PerceivedThing item;
    for (auto iterator = get_perceived_items(individual); iterator.next(&item);)
        if (item->location == individual->location)
            output_actions.append(Action::pickup(item->id));
    // use items
    List<Thing> inventory;
    find_items_in_inventory(individual->id, &inventory);
    for (int i = 0; i < inventory.length(); i++) {
        uint256 item_id = inventory[i]->id;
        for (int j = 0; j < 8; j++) {
            Coord direction = directions[j];
            output_actions.append(Action::zap(item_id, direction));
            output_actions.append(Action::throw_(item_id, direction));
        }
        output_actions.append(Action::drop(item_id));
    }
    // go down
    if (actual_map_tiles[individual->location].tile_type == TileType_STAIRS_DOWN)
        output_actions.append(Action::go_down());

    // alright, we'll let you use cheatcodes
    if (individual == you) {
        output_actions.append(Action::cheatcode_health_boost());
        output_actions.append(Action::cheatcode_kill_everybody_in_the_world());
        output_actions.append(Action::cheatcode_polymorph());
        output_actions.append(Action::cheatcode_invisibility());
        output_actions.append(Action::cheatcode_generate_monster());
        output_actions.append(Action::cheatcode_create_item());
        output_actions.append(Action::cheatcode_go_down());
        output_actions.append(Action::cheatcode_gain_level());
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

void fix_z_orders(uint256 container_id) {
    List<Thing> inventory;
    find_items_in_inventory(container_id, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        inventory[i]->z_order = i;
}
void fix_z_orders(Coord location) {
    List<Thing> inventory;
    find_items_on_floor(location, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        inventory[i]->z_order = i;
}
