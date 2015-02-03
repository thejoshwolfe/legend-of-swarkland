#include "swarkland.hpp"

#include "path_finding.hpp"

Species specieses[SpeciesId_COUNT];
LinkedHashtable<uint256, Individual, hash_uint256> individuals;

Individual you;
long long time_counter = 0;

bool cheatcode_full_visibility;

static void init_specieses() {
    specieses[SpeciesId_HUMAN] = {SpeciesId_HUMAN, 12, 10, 3, AiStrategy_ATTACK_IF_VISIBLE, {1, 0}, true};
    specieses[SpeciesId_OGRE] = {SpeciesId_OGRE, 24, 10, 2, AiStrategy_ATTACK_IF_VISIBLE, {1, 0}, true};
    specieses[SpeciesId_DOG] = {SpeciesId_DOG, 12, 4, 2, AiStrategy_ATTACK_IF_VISIBLE, {1, 0}, true};
    specieses[SpeciesId_GELATINOUS_CUBE] = {SpeciesId_GELATINOUS_CUBE, 48, 12, 4, AiStrategy_ATTACK_IF_VISIBLE, {0, 1}, false};
    specieses[SpeciesId_DUST_VORTEX] = {SpeciesId_DUST_VORTEX, 6, 6, 1, AiStrategy_ATTACK_IF_VISIBLE, {0, 1}, false};
}

static const int no_spawn_radius = 10;

Individual spawn_a_monster(SpeciesId species_id) {
    while (species_id == SpeciesId_COUNT) {
        species_id = (SpeciesId)random_int(SpeciesId_COUNT);
        if (species_id == SpeciesId_HUMAN) {
            // humans are too hard. without giving one side a powerup, they're evenly matched.
            species_id = SpeciesId_COUNT;
        }
    }
    List<Coord> available_spawn_locations;
    for (Coord location(0, 0); location.y < map_size.y; location.y++) {
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
    Coord location = available_spawn_locations.at(random_int(available_spawn_locations.size()));
    Individual individual = new IndividualImpl(species_id, location);
    individuals.put(individual->id, individual);
    return individual;
}

static void init_individuals() {
    you = spawn_a_monster(SpeciesId_HUMAN);
    if (you == NULL)
        panic("can't spawn you");
    you->ai = AiStrategy_PLAYER;

    // generate a few warm-up monsters
    for (int i = 0; i < 6; i++) {
        spawn_a_monster(SpeciesId_COUNT);
    }
}

void swarkland_init() {
    init_specieses();

    generate_map();

    init_individuals();
}

static void spawn_monsters() {
    if (random_int(120) == 0)
        spawn_a_monster(SpeciesId_COUNT);
}
static void heal_the_living() {
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual->hitpoints < individual->species->starting_hitpoints) {
            if (random_int(60) == 0) {
                individual->hitpoints++;
            }
        }
    }
}

static void attack(Individual attacker, Individual target) {
    target->hitpoints -= attacker->species->attack_power;
    if (target->hitpoints <= 0) {
        target->hitpoints = 0;
        target->is_alive = false;
        attacker->kill_counter++;
    }
}

static void move_individual(Individual individual, Coord new_position) {
    if (new_position.x == you->location.x && new_position.y == you->location.y) {
        attack(individual, you);
    } else {
        individual->location = new_position;
    }
}

static void move_bumble_around(Individual individual) {
    if (individual->knowledge.is_visible[you->location].any() && !you->invisible) {
        // there he is!
        List<Coord> path;
        find_path(individual->location, you->location, individual, path);
        if (path.size() > 0) {
            move_individual(individual, path.at(0));
        } else {
            // we must be stuck in a crowd
            return;
        }

        // if we lose him. reroll our new destination.
        individual->bumble_destination = Coord(-1, -1);
    } else {
        // idk where 2 go
        if (individual->bumble_destination == Coord(-1, -1))
            individual->bumble_destination = individual->location;
        Coord next_space = individual->location + sign(individual->bumble_destination - individual->location);
        if (individual->bumble_destination == individual->location || !do_i_think_i_can_move_here(individual, next_space)) {
            // we've just arrived, or we need to pick a new target.
            // head off in a random direction.
            List<Coord> available_immediate_vectors;
            for (int i = 0; i < 8; i++) {
                Coord direction = directions[i];
                Coord adjacent_space(individual->location.x + direction.x, individual->location.y + direction.y);
                if (do_i_think_i_can_move_here(individual, adjacent_space))
                    available_immediate_vectors.add(direction);
            }
            if (available_immediate_vectors.size() == 0) {
                // we're stuck. do nothing.
                return;
            }
            Coord direction = available_immediate_vectors.at(random_int(available_immediate_vectors.size()));
            // pick a random distance to travel, within reason.
            int distance = random_int(1, 6);
            Coord destination = individual->location;
            for (int i = 0; i < distance; i++) {
                // don't set a destination out of bounds
                Coord next_space = destination + direction;
                if (!is_in_bounds(next_space))
                    break;
                destination = next_space;
            }
            individual->bumble_destination = destination;
            next_space = individual->location + sign(individual->bumble_destination - individual->location);
        }
        // we have somewhere to go
        move_individual(individual, next_space);
    }
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

static void move_with_ai(Individual individual) {
    refresh_vision(individual);
    if (individual->ai != AiStrategy_ATTACK_IF_VISIBLE)
        panic("unknown ai strategy");
    move_bumble_around(individual);
}
bool take_action(bool just_wait, Coord delta) {
    refresh_vision(you);
    if (cheatcode_spectator != NULL) {
        // doing this early shouldn't affect anything
        refresh_vision(cheatcode_spectator);
    }

    if (!you->is_alive)
        return false;
    if (just_wait)
        return true;
    if (delta.x == 0 && delta.y == 0)
        return false; // not moving
    Coord new_position(you->location.x + delta.x, you->location.y + delta.y);
    if (new_position.x < 0 || new_position.x >= map_size.x || new_position.y < 0 || new_position.y >= map_size.y)
        return false;
    if (you->knowledge.tiles[new_position].tile_type == TileType_WALL)
        return false;
    Individual target = find_individual_at(new_position);
    if (target != NULL && target != you) {
        attack(you, target);
    } else {
        you->location = new_position;
    }
    return true;
}

void advance_time() {
    time_counter++;

    spawn_monsters();
    heal_the_living();

    // award movement points to the living
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        individual->movement_points++;
    }

    // move monsters
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual->ai == AiStrategy_PLAYER)
            continue;
        if (individual->movement_points >= individual->species->movement_cost) {
            // make a move
            individual->movement_points = 0;
            move_with_ai(individual);
        }
    }
}

void cheatcode_kill_everybody_in_the_world() {
    for (auto iterator = individuals.value_iterator(); iterator.has_next();)
        iterator.next()->is_alive = false;
    // you're cool
    you->is_alive = true;
}
void cheatcode_polymorph() {
    SpeciesId species_id = you->species->species_id;
    species_id = (SpeciesId)((species_id + 1) % SpeciesId_COUNT);
    you->species = &specieses[species_id];
}
Individual cheatcode_spectator;
void cheatcode_spectate(Coord individual_at) {
    for (auto iterator = individuals.value_iterator(); iterator.has_next();) {
        Individual individual = iterator.next();
        if (!individual->is_alive)
            continue;
        if (individual->location == individual_at) {
            cheatcode_spectator = individual;
            return;
        }
    }
    cheatcode_spectator = NULL;
}
