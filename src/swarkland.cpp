#include "swarkland.hpp"

#include "path_finding.hpp"
#include "decision.hpp"
#include "display.hpp"
#include "event.hpp"

Species specieses[SpeciesId_COUNT];
IdMap<Individual> actual_individuals;
IdMap<Item> actual_items;

Individual you;
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

static const int no_spawn_radius = 10;

// specify SpeciesId_COUNT for random
Individual spawn_a_monster(SpeciesId species_id, Team team, DecisionMakerType decision_maker) {
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

    Individual individual = new IndividualImpl(species_id, location, team, decision_maker);

    if (random_int(1) == 0) {
        // have an item
        Item item = random_item();
        individual->inventory.append(item);
    }

    actual_individuals.put(individual->id, individual);
    compute_vision(individual, true);
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

static void regen_hp(Individual individual) {
    if (individual->hitpoints < individual->species()->starting_hitpoints) {
        if (random_int(60) == 0) {
            individual->hitpoints++;
        }
    }
}

static void kill_individual(Individual individual) {
    individual->hitpoints = 0;
    individual->is_alive = false;

    publish_event(Event::die(individual));

    if (individual == you)
        youre_still_alive = false;
    if (individual == cheatcode_spectator) {
        // our fun looking through the eyes of a dying man has ended. back to normal.
        cheatcode_spectator = NULL;
    }
}

static void damage_individual(Individual attacker, Individual target, int damage) {
    if (damage <= 0)
        panic("no damage");
    target->hitpoints -= damage;
    if (target->hitpoints <= 0) {
        // take his items!!!
        for (int i = 0; i < target->inventory.length(); i++)
            attacker->inventory.append(target->inventory[i]);
        kill_individual(target);
        attacker->kill_counter++;
    }
}

// normal melee attack
static void attack(Individual attacker, Individual target) {
    publish_event(Event::attack(attacker, target));
    damage_individual(attacker, target, attacker->species()->attack_power);
}

PerceivedIndividual find_perceived_individual_at(Individual observer, Coord location) {
    for (auto iterator = observer->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual individual = iterator.next();
        if (individual->location == location)
            return individual;
    }
    return NULL;
}
Individual find_individual_at(Coord location) {
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual->location.x == location.x && individual->location.y == location.y)
            return individual;
    }
    return NULL;
}

static void do_move(Individual mover, Coord new_position) {
    Coord old_position = mover->location;
    mover->location = new_position;
    compute_vision(mover, false);

    // notify other individuals who could see that move
    publish_event(Event::move(mover, old_position, new_position));
}

static void cheatcode_kill_everybody_in_the_world() {
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual != you)
            kill_individual(individual);
    }
}
static void cheatcode_polymorph() {
    SpeciesId old_species = you->species_id;
    you->species_id = (SpeciesId)((old_species + 1) % SpeciesId_COUNT);
    compute_vision(you, false);
    publish_event(Event::polymorph(you, old_species));
}
Individual cheatcode_spectator;
void cheatcode_spectate() {
    Coord individual_at = get_mouse_tile(main_map_area);
    cheatcode_spectator = find_individual_at(individual_at);
}

static bool validate_action(Individual individual, Action action) {
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
Coord confuse_direction(Individual individual, Coord direction) {
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
static bool take_action(Individual actor, Action action) {
    bool is_valid = validate_action(actor, action);
    if (!is_valid) {
        if (actor == you) {
            // forgive the player for trying to run into a wall or something
            return false;
        }
        panic("ai tried to make an illegal move");
    }

    // we know you can attempt the action, but it won't necessarily turn out the way you expected it.
    actor->movement_points = 0;

    switch (action.type) {
        case Action::WAIT:
            return true;
        case Action::MOVE: {
            // normally, we'd be sure that this was valid, but if you use cheatcodes,
            // monsters can try to walk into you while you're invisible.
            Coord new_position = actor->location + confuse_direction(actor, action.coord);
            if (!is_in_bounds(new_position) || actual_map_tiles[new_position].tile_type == TileType_WALL) {
                // this can only happen if your direction was changed, since attempting to move into a wall is invalid.
                publish_event(Event::bump_into_wall(actor, new_position));
                return true;
            }
            Individual target = find_individual_at(new_position);
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
            Individual target = find_individual_at(new_position);
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

        case Action::CHEATCODE_HEALTH_BOOST:
            actor->hitpoints += 100;
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
        default:
            panic("unimplemented action type");
    }
}

List<Individual> poised_individuals;
int poised_individuals_index = 0;
// this function will return only when we're expecting player input
void run_the_game() {
    while (youre_still_alive) {
        if (poised_individuals.length() == 0) {
            time_counter++;

            spawn_monsters(false);

            List<Individual> dead_individuals;
            List<Event> deferred_events;
            // who's ready to make a move?
            for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
                Individual individual = iterator.next();
                if (!individual->is_alive) {
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
                individual->movement_points++;
                if (individual->movement_points >= individual->species()->movement_cost) {
                    poised_individuals.append(individual);
                    // log the passage of time in the message window.
                    // this actually only observers time in increments of your movement cost
                    if (individual->species()->has_mind) {
                        List<RememberedEvent> & events = individual->knowledge.remembered_events;
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
                actual_individuals.remove(dead_individuals[i]->id);

            // who really gets to go first is determined by initiative
            sort<Individual, compare_individuals_by_initiative>(poised_individuals.raw(), poised_individuals.length());
        }

        // move individuals
        for (; poised_individuals_index < poised_individuals.length(); poised_individuals_index++) {
            Individual individual = poised_individuals[poised_individuals_index];
            if (!individual->is_alive)
                continue; // sorry, buddy. you were that close to making another move.
            Action action = decision_makers[individual->decision_maker](individual);
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
void get_available_actions(Individual individual, List<Action> & output_actions) {
    output_actions.append(Action::wait());
    // move
    for (int i = 0; i < 8; i++) {
        Coord direction = directions[i];
        if (do_i_think_i_can_move_here(individual, individual->location + direction))
            output_actions.append(Action::move(direction));
    }
    // attack
    for (auto iterator = individual->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual target = iterator.next();
        if (target->id == individual->id)
            continue; // you can't attack yourself, sorry.
        Coord vector = target->location - individual->location;
        if (vector == sign(vector)) {
            // within melee range
            output_actions.append(Action::attack(vector));
        }
    }
    // use items
    for (int i = 0; i < individual->inventory.length(); i++)
        for (int j = 0; j < 8; j++)
            output_actions.append(Action::zap(individual->inventory[i], directions[j]));

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
bool confuse_individual(Individual target) {
    if (!target->species()->has_mind) {
        // can't confuse something with no mind
        return false;
    }
    target->status_effects.confused_timeout = random_int(100, 200);
    return true;
}

void strike_individual(Individual attacker, Individual target) {
    // it's just some damage
    int damage = random_int(4, 8);
    damage_individual(attacker, target, damage);
}
void change_map(Coord location, TileType new_tile_type) {
    actual_map_tiles[location].tile_type = new_tile_type;
    // recompute everyone's vision
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        compute_vision(individual, true);
    }
}
