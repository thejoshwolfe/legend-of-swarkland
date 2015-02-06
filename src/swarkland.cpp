#include "swarkland.hpp"

#include "path_finding.hpp"
#include "decision.hpp"

Species specieses[SpeciesId_COUNT];
IdMap<Individual> individuals;

Individual you;
bool youre_still_alive = true;
long long time_counter = 0;

bool cheatcode_full_visibility;

static void init_specieses() {
    specieses[SpeciesId_HUMAN] = {SpeciesId_HUMAN, 12, 10, 3, {1, 0}, true};
    specieses[SpeciesId_OGRE] = {SpeciesId_OGRE, 24, 10, 2, {1, 0}, true};
    specieses[SpeciesId_DOG] = {SpeciesId_DOG, 12, 4, 2, {1, 0}, true};
    specieses[SpeciesId_GELATINOUS_CUBE] = {SpeciesId_GELATINOUS_CUBE, 48, 12, 4, {0, 1}, false};
    specieses[SpeciesId_DUST_VORTEX] = {SpeciesId_DUST_VORTEX, 6, 6, 1, {0, 1}, false};
}

static void publish_event(Event event) {
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual observer = iterator.next();
        if (!observer->is_alive)
            continue;
        bool can_see1 = observer->knowledge.tile_is_visible[event.coord1].any();
        bool can_see2 = event.coord2 != Coord::nowhere() && observer->knowledge.tile_is_visible[event.coord2].any();
        if (!(can_see1 || can_see2))
            continue; // out of view
        // i see what happened
        if (event.individual1 != NULL) {
            if (event.type == Event::DIE) {
                observer->knowledge.perceived_individuals.remove(event.individual1->id);
            } else if (event.type == Event::DISAPPEAR) {
                // you can see see yourself
                if (observer != event.individual1)
                    observer->knowledge.perceived_individuals.remove(event.individual1->id);
            } else {
                observer->knowledge.perceived_individuals.put(event.individual1->id, to_perceived_individual(event.individual1));
            }
        }
        if (event.individual2 != NULL)
            observer->knowledge.perceived_individuals.put(event.individual2->id, to_perceived_individual(event.individual2));
    }

    if (event.type == Event::DIE) {
        // we didn't notice ourselves dying in the above loop
        event.individual1->knowledge.perceived_individuals.remove(event.individual1->id);
    }
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
            available_spawn_locations.add(location);
        }
    }
    if (available_spawn_locations.size() == 0) {
        // it must be pretty crowded in here
        return NULL;
    }
    Coord location = available_spawn_locations[random_int(available_spawn_locations.size())];
    Individual individual = new IndividualImpl(species_id, location, team, decision_maker);
    individuals.put(individual->id, individual);
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
    if (individual->hitpoints < individual->species->starting_hitpoints) {
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

static void attack(Individual attacker, Individual target) {
    target->hitpoints -= attacker->species->attack_power;
    publish_event(Event::attack(attacker, target));
    if (target->hitpoints <= 0) {
        kill_individual(target);
        attacker->kill_counter++;
    }
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
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
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
    compute_vision(mover);

    // notify other individuals who could see that move
    publish_event(Event::move(mover, old_position, new_position));
}

static void cheatcode_kill_everybody_in_the_world() {
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual != you)
            kill_individual(individual);
    }
}
static void cheatcode_polymorph() {
    SpeciesId species_id = you->species->species_id;
    species_id = (SpeciesId)((species_id + 1) % SpeciesId_COUNT);
    you->species = &specieses[species_id];
    compute_vision(you);
    publish_event(Event::polymorph(you));
}
Individual cheatcode_spectator;
void cheatcode_spectate(Coord individual_at) {
    cheatcode_spectator = find_individual_at(individual_at);
}

static bool validate_action(Individual individual, Action action) {
    List<Action> valid_actions;
    get_available_actions(individual, valid_actions);
    for (int i = 0; i < valid_actions.size(); i++)
        if (valid_actions[i] == action)
            return true;
    return false;
}

// return whether we did anything. also, cheatcodes take no time
static bool take_action(Individual individual, Action action) {
    bool is_valid = validate_action(individual, action);
    if (!is_valid) {
        if (individual == you) {
            // forgive the player for trying to run into a wall or something
            return false;
        }
        panic("ai tried to make an illegal move");
    }

    individual->movement_points = 0;

    switch (action.type) {
        case Action::WAIT:
            return true;
        case Action::MOVE: {
            // normally, we'd be sure that this was valid, but if you use cheatcodes,
            // monsters can try to walk into you while you're invisible.
            Coord new_position = individual->location + action.coord;
            Individual unseen_obstacle = find_individual_at(new_position);
            if (unseen_obstacle != NULL) {
                // someday, this will cause you to notice that there's an invisible monster there,
                // but for now, just lose a turn.
                return true;
            }
            // clear to move
            do_move(individual, new_position);
            return true;
        }
        case Action::ATTACK: {
            Coord new_position = individual->location + action.coord;
            Individual target = find_individual_at(new_position);
            if (target == NULL) {
                // you attack thin air
                return true;
            }
            attack(individual, target);
            return true;
        }
        case Action::CHEATCODE_HEALTH_BOOST:
            individual->hitpoints += 100;
            return false;
        case Action::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD:
            cheatcode_kill_everybody_in_the_world();
            return false;
        case Action::CHEATCODE_POLYMORPH:
            cheatcode_polymorph();
            // this one does take time, because your movement cost may have changed
            return true;
        case Action::CHEATCODE_INVISIBILITY:
            if (individual->invisible) {
                individual->invisible = false;
                publish_event(Event::disappear(you));
            } else {
                individual->invisible = true;
                publish_event(Event::appear(you));
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
        if (poised_individuals.size() == 0) {
            time_counter++;

            spawn_monsters(false);

            List<Individual> dead_individuals;
            // who's ready to make a move?
            for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
                Individual individual = iterator.next();
                if (!individual->is_alive) {
                    dead_individuals.add(individual);
                    continue;
                }
                // advance time for this individual
                regen_hp(individual);
                individual->movement_points++;
                if (individual->movement_points >= individual->species->movement_cost)
                    poised_individuals.add(individual);
            }
            // delete the dead
            for (int i = 0; i < dead_individuals.size(); i++) {
                individuals.remove(dead_individuals[i]->id);
            }

            // break ties with randomly assigned initiative
            sort<Individual, compare_individuals_by_initiative>(poised_individuals.raw(), poised_individuals.size());
        }

        // move individuals
        for (; poised_individuals_index < poised_individuals.size(); poised_individuals_index++) {
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
    output_actions.add(Action::wait());
    // move
    for (int i = 0; i < 8; i++) {
        Coord direction = directions[i];
        if (do_i_think_i_can_move_here(individual, individual->location + direction))
            output_actions.add(Action{Action::MOVE, direction});
    }
    // attack
    for (auto iterator = individual->knowledge.perceived_individuals.value_iterator(); iterator.has_next();) {
        PerceivedIndividual target = iterator.next();
        if (target->id == individual->id)
            continue; // you can't attack yourself, sorry.
        Coord vector = target->location - individual->location;
        if (vector == sign(vector)) {
            // within melee range
            output_actions.add(Action{Action::ATTACK, vector});
        }
    }
    // alright, we'll let you use cheatcodes
    if (individual == you) {
        output_actions.add(Action::cheatcode_health_boost());
        output_actions.add(Action::cheatcode_kill_everybody_in_the_world());
        output_actions.add(Action::cheatcode_polymorph());
        output_actions.add(Action::cheatcode_invisibility());
        output_actions.add(Action::cheatcode_generate_monster());
    }
}
