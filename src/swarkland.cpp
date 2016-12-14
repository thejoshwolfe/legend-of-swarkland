#include <serial.hpp>
#include "swarkland.hpp"

#include "path_finding.hpp"
#include "decision.hpp"
#include "display.hpp"
#include "event.hpp"
#include "item.hpp"

bool print_diagnostics;
int snapshot_interval = 500;
bool headless_mode;

Game * game;
bool cheatcode_full_visibility;
Thing cheatcode_spectator;

static void init_static_data() {
    // shorthand
    for (int i = 0; i < SpeciesId_COUNT; i++)
        if (specieses[i].movement_cost == 0)
            panic("you missed a spot");
}

static void kill_individual(Thing individual, Thing attacker, bool is_melee) {
    individual->life()->hitpoints = 0;

    if (is_melee) {
        publish_event(Event::melee_kill(attacker, individual));
    } else {
        publish_event(Event::die(individual->id));
    }
    individual->still_exists = false;

    // drop your stuff
    List<Thing> inventory;
    find_items_in_inventory(individual->id, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        drop_item_to_the_floor(inventory[i], individual->location);

    if (individual == cheatcode_spectator) {
        // our fun looking through the eyes of a dying man has ended. back to normal.
        cheatcode_spectator = nullptr;
    }
}

static void level_up(Thing individual, bool publish) {
    Life * life = individual->life();
    int old_max_hitpoints = individual->max_hitpoints();
    int old_max_mana = individual->max_mana();
    life->experience = individual->next_level_up();
    int new_max_hitpoints = individual->max_hitpoints();
    int new_max_mana = individual->max_mana();
    life->hitpoints = life->hitpoints * new_max_hitpoints / old_max_hitpoints;
    if (old_max_mana > 0)
        life->mana = life->mana * new_max_mana / old_max_mana;
    else
        life->mana = new_max_mana;
    if (publish)
        publish_event(Event::level_up(individual->id));
}

static void gain_experience(Thing individual, int delta, bool publish) {
    Life * life = individual->life();
    int new_expderience = life->experience + delta;
    while (new_expderience >= individual->next_level_up())
        level_up(individual, publish);
    life->experience = new_expderience;
}

static void reset_hp_regen_timeout(Thing individual) {
    Life * life = individual->life();
    if (!game->test_mode && life->hitpoints < individual->max_hitpoints())
        life->hp_regen_deadline = game->time_counter + random_midpoint(7 * 12, nullptr);
}
void damage_individual(Thing target, int damage, Thing attacker, bool is_melee) {
    assert_str(damage > 0, "no damage");
    target->life()->hitpoints -= damage;
    reset_hp_regen_timeout(target);
    if (target->life()->hitpoints <= 0) {
        kill_individual(target, attacker, is_melee);
        if (attacker != nullptr && attacker->id != target->id)
            gain_experience(attacker, 1, true);
    }
}

void poison_individual(Thing attacker, Thing target) {
    publish_event(Event::gain_status(target->id, StatusEffect::POISON));

    StatusEffect * poison = find_or_put_status(target, StatusEffect::POISON);
    poison->who_is_responsible = attacker->id;
    poison->expiration_time = game->time_counter + random_midpoint(600, "poison_expiriration");
    poison->poison_next_damage_time = game->time_counter + 12 * 3;
}
static void blind_individual(Thing attacker, Thing target) {
    StatusEffect * effect = find_or_put_status(target, StatusEffect::BLINDNESS);
    effect->who_is_responsible = attacker->id;
    effect->expiration_time = game->time_counter + random_midpoint(600, "blindness_expiriration");
}
void slow_individual(Thing actor, Thing target) {
    StatusEffect * effect = find_or_put_status(target, StatusEffect::SLOWING);
    effect->who_is_responsible = actor->id;
    effect->expiration_time = game->time_counter + random_midpoint(200, "slowing_duration");
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
    if (individual != nullptr && individual->physical_species()->sucks_up_items) {
        // suck it
        pickup_item(individual, item);
        publish_event(Event::individual_sucks_up_item(individual->id, item->id));
    }
}
static void throw_item(Thing actor, Thing item, Coord direction) {
    publish_event(Event::throw_item(actor->id, item->id));
    // let go of the item. it's now sailing through the air.
    item->location = actor->location;
    item->container_id = uint256::zero();
    item->z_order = 0;
    fix_z_orders(actor->id);

    // find the hit target
    int range = random_int(throw_distance_average - throw_distance_error_margin, throw_distance_average + throw_distance_error_margin, "throw_distance");
    Coord cursor = actor->location;
    bool item_breaks = false;
    // potions are fragile
    if (item->thing_type == ThingType_POTION)
        item_breaks = true;
    bool impacts_in_wall = item->thing_type == ThingType_WAND && item->wand_info()->wand_id == WandId_WAND_OF_DIGGING;
    for (int i = 0; i < range; i++) {
        cursor += direction;
        if (!is_open_space(game->actual_map_tiles[cursor])) {
            if (!(item_breaks && impacts_in_wall)) {
                // impact just in front of the wall
                cursor -= direction;
            }
            item_breaks = item->thing_type == ThingType_POTION || random_int(2, "wand_breaks") == 0;
            publish_event(Event::item_hits_wall(item->id, cursor));
            break;
        } else {
            item->location = cursor;
            publish_event(Event::move(item, cursor - direction));
        }
        Thing target = find_individual_at(cursor);
        if (target != nullptr) {
            // wham!
            publish_event(Event::item_hits_individual(target->id, item->id));
            // hurt a little
            int damage = random_inclusive(1, 2, "throw_impact_damage");
            damage_individual(target, damage, actor, false);
            if (damage == 2) {
                // no item can survive that much damage dealt
                item_breaks = true;
            }
            break;
        }
    }

    if (item_breaks) {
        switch (item->thing_type) {
            case ThingType_INDIVIDUAL:
                unreachable();
            case ThingType_WAND:
                explode_wand(actor, item, cursor);
                break;
            case ThingType_POTION:
                break_potion(actor, item, cursor);
                break;
            case ThingType_BOOK:
                // actually, the book doesn't break
                drop_item_to_the_floor(item, cursor);
                break;

            case ThingType_COUNT:
                unreachable();
        }
    } else {
        drop_item_to_the_floor(item, cursor);
    }
}

static void do_ability(Thing actor, AbilityId ability_id, Coord direction) {
    int range;
    int cooldown_time;
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            publish_event(Event::spit_blinding_venom(actor->id));
            cooldown_time = random_midpoint(150, "spit_blinding_venom_cooldown");
            range = random_int(throw_distance_average - throw_distance_error_margin, throw_distance_average + throw_distance_error_margin, "spit_blinding_venom_distance");
            break;
        case AbilityId_THROW_TAR:
            publish_event(Event::throw_tar(actor->id));
            cooldown_time = random_midpoint(150, "throw_tar_cooldown");
            range = random_int(throw_distance_average - throw_distance_error_margin, throw_distance_average + throw_distance_error_margin, "throw_tar_distance");
            break;
        case AbilityId_ASSUME_FORM:
            cooldown_time = random_midpoint(48, "assume_form_cooldown");
            range = infinite_range;
            break;
        case AbilityId_COUNT:
            unreachable();
    }

    if (cooldown_time > 0)
        actor->ability_cooldowns.append(AbilityCooldown{ability_id, game->time_counter + cooldown_time});

    Coord cursor = actor->location;
    for (int i = 0; i < range; i++) {
        cursor += direction;
        Thing target = find_individual_at(cursor);
        if (target != nullptr) {
            switch (ability_id) {
                case AbilityId_SPIT_BLINDING_VENOM:
                    publish_event(Event::blinding_venom_hit_individual(target->id));
                    publish_event(Event::gain_status(target->id, StatusEffect::BLINDNESS));
                    blind_individual(actor, target);
                    compute_vision(target);
                    return;
                case AbilityId_THROW_TAR:
                    publish_event(Event::tar_hit_individual(target->id));
                    publish_event(Event::gain_status(target->id, StatusEffect::SLOWING));
                    slow_individual(actor, target);
                    return;
                case AbilityId_ASSUME_FORM:
                    polymorph_individual(actor, target->physical_species_id(), random_midpoint(240, "ability_assume_form_duration"));
                    return;
                case AbilityId_COUNT:
                    unreachable();
            }
            unreachable();
        }
        if (!is_open_space(game->actual_map_tiles[cursor])) {
            // hit wall
            // TODO: publish event
            return;
        }
    }
    // falls on the floor
}

static Coord find_stairs_down_location() {
    if (game->dungeon_level == final_dungeon_level)
        return Coord::nowhere();
    for (Coord location = {0, 0}; location.y < map_size.y; location.y++)
        for (location.x = 0; location.x < map_size.x; location.x++)
            if (game->actual_map_tiles[location] == TileType_STAIRS_DOWN)
                return location;
    unreachable();
}

static inline uint256 random_initiative() {
    if (game->test_mode) {
        // just increment a counter
        game->random_initiative_count.values[3]++;
        return game->random_initiative_count;
    }
    return random_uint256();
}

// SpeciesId_COUNT => random
// location = Coord::nowhere() => random
static Thing spawn_a_monster(SpeciesId species_id, DecisionMakerType decision_maker, Coord location) {
    if (species_id == SpeciesId_COUNT) {
        int difficulty = game->dungeon_level - random_triangle_distribution(game->dungeon_level);
        assert(0 <= difficulty && difficulty < game->dungeon_level);
        List<SpeciesId> available_specieses;
        for (int i = 0; i < SpeciesId_COUNT; i++) {
            if (i == SpeciesId_HUMAN)
                continue; // humans are excluded from random
            const Species & species = specieses[i];
            if (species.min_level == difficulty)
                available_specieses.append((SpeciesId)i);
        }
        assert(available_specieses.length() != 0);
        species_id = available_specieses[random_int(available_specieses.length(), nullptr)];
    }

    if (location == Coord::nowhere()) {
        // don't spawn monsters near you.
        Coord away_from_location = game->you_id != uint256::zero() ? you()->location : find_stairs_down_location();
        location = random_spawn_location(away_from_location);
        if (location == Coord::nowhere()) {
            // it must be pretty crowded in here
            return nullptr;
        }
    }

    Thing individual = create<ThingImpl>(random_id(), species_id, decision_maker, random_initiative());
    individual->location = location;

    gain_experience(individual, level_to_experience(specieses[species_id].min_level + 1) - 1, false);

    game->actual_things.put(individual->id, individual);
    compute_vision(individual);
    publish_event(Event::appear(individual));
    return individual;
}

static SpeciesId get_miniboss_species(int dungeon_level) {
    switch (dungeon_level) {
        case 1: return SpeciesId_DOG;
        case 2: return SpeciesId_SCORPION;
        case 3: return SpeciesId_AIR_ELEMENTAL;
        case 4: return SpeciesId_OGRE;
    }
    panic("dungeon_level");
}

static void spawn_random_individual() {
    spawn_a_monster(SpeciesId_COUNT, DecisionMakerType_AI, Coord::nowhere());
}
static void init_individuals() {
    if (game->test_mode) {
        game->you_id = spawn_a_monster(SpeciesId_HUMAN, DecisionMakerType_PLAYER, Coord{1, 1})->id;
        return;
    }

    if (game->you_id == uint256::zero()) {
        game->you_id = spawn_a_monster(SpeciesId_HUMAN, DecisionMakerType_PLAYER, Coord::nowhere())->id;
    } else {
        // you just landed from upstairs
        // make sure the up and down stairs are sufficiently far apart.
        you()->location = random_spawn_location(find_stairs_down_location());
        compute_vision(you());
    }

    if (game->dungeon_level == final_dungeon_level) {
        // boss
        Thing boss = spawn_a_monster(SpeciesId_LICH, DecisionMakerType_AI, Coord::nowhere());
        // arm him!
        for (int i = 0; i < 5; i++) {
            pickup_item(boss, create_random_item(ThingType_WAND));
            pickup_item(boss, create_random_item(ThingType_POTION));
        }
        // teach him everything about magic.
        for (int i = 0; i < WandId_COUNT; i++)
            boss->life()->knowledge.wand_identities[game->actual_wand_descriptions[i]] = (WandId)i;
        for (int i = 0; i < PotionId_COUNT; i++)
            boss->life()->knowledge.potion_identities[game->actual_potion_descriptions[i]] = (PotionId)i;
        for (int i = 0; i < BookId_COUNT; i++)
            boss->life()->knowledge.book_identities[game->actual_book_descriptions[i]] = (BookId)i;
    } else {
        // spawn a "miniboss", which is just a specific monster on the stairs.
        SpeciesId miniboss_species_id = get_miniboss_species(game->dungeon_level);
        spawn_a_monster(miniboss_species_id, DecisionMakerType_AI, find_stairs_down_location());
    }

    // random monsters
    for (int i = 0; i < 4 + 3 * game->dungeon_level; i++)
        spawn_a_monster(SpeciesId_COUNT, DecisionMakerType_AI, Coord::nowhere());
}

void swarkland_init() {
    init_static_data();
    init_items();

    generate_map();

    init_individuals();
}

void go_down() {
    // goodbye everyone
    Thing thing;
    for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);) {
        if (thing == you())
            continue; // you're cool
        if (thing->container_id == you()->id)
            continue; // take it with you
        // leave this behind us
        thing->still_exists = false;
    }

    you()->life()->knowledge.reset_map();
    generate_map();
    init_individuals();
}

static void maybe_spawn_monsters() {
    if (game->test_mode)
        return;
    if (random_int(2000, nullptr) == 0)
        spawn_random_individual();
}

void heal_hp(Thing individual, int hp) {
    Life * life = individual->life();
    if (life->mana < 0) {
        // no can do.
        return;
    }
    life->hitpoints = min(life->hitpoints + hp, individual->max_hitpoints());
    reset_hp_regen_timeout(individual);
}
static void regen_hp(Thing individual) {
    Life * life = individual->life();
    int index;
    if ((index = find_status(individual->status_effects, StatusEffect::POISON)) != -1) {
        // poison damage instead
        StatusEffect * poison = &individual->status_effects[index];
        if (poison->poison_next_damage_time == game->time_counter) {
            // ouch
            Thing attacker = game->actual_things.get(poison->who_is_responsible, nullptr);
            if (attacker != nullptr && !attacker->still_exists)
                attacker = nullptr;
            damage_individual(individual, 1, attacker, false);
            poison->poison_next_damage_time = game->time_counter + random_midpoint(7 * 12, "poison_damage");
        }
    } else if (life->hp_regen_deadline == game->time_counter) {
        // hp regen
        int hp_heal = random_inclusive(1, max(1, individual->max_hitpoints() / 5), nullptr);
        heal_hp(individual, hp_heal);
    }
}

static void reset_mp_regen_timeout(Thing individual) {
    Life * life = individual->life();
    if (!game->test_mode && life->mana < individual->max_mana())
        life->mp_regen_deadline = game->time_counter + random_midpoint(7 * 12, nullptr);
}
void gain_mp(Thing individual, int mp) {
    Life * life = individual->life();
    bool was_negative = life->mana < 0;
    life->mana = min(life->mana + mp, individual->max_mana());
    reset_mp_regen_timeout(individual);

    if (was_negative && life->mana >= 0) {
        // you can heal again
        reset_hp_regen_timeout(individual);
    }
}
static void regen_mp(Thing individual) {
    Life * life = individual->life();
    if (life->mp_regen_deadline == game->time_counter) {
        int mp_gain = random_inclusive(1, max(1, individual->max_mana() / 5), nullptr);
        gain_mp(individual, mp_gain);
    }
}
void use_mana(Thing actor, int mana) {
    assert(mana > 0);
    Life * life = actor->life();
    life->mana -= mana;
    if (life->mana < 0) {
        // also lose hp
        int damage = min(-life->mana, mana);
        damage_individual(actor, damage, actor, false);
    }
    reset_mp_regen_timeout(actor);
}


// normal melee attack
static void attack(Thing attacker, Thing target) {
    publish_event(Event::attack_individual(attacker, target));
    int attack_power = attacker->attack_power();
    int min_damage = (attack_power + 1) / 2;
    int damage = random_inclusive(min_damage, min_damage + attack_power / 2, "melee_damage");
    damage_individual(target, damage, attacker, true);
    reset_hp_regen_timeout(attacker);
    if (target->still_exists && attacker->physical_species()->poison_attack && random_int(4, "poison_attack") == 0)
        poison_individual(attacker, target);
}

static int compare_things_by_z_order(Thing a, Thing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
static int compare_perceived_things_by_z_order(PerceivedThing a, PerceivedThing b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
int compare_perceived_things_by_type_and_z_order(PerceivedThing a, PerceivedThing b) {
    int is_individual_a = a->thing_type == ThingType_INDIVIDUAL;
    int is_individual_b = b->thing_type == ThingType_INDIVIDUAL;
    int result = is_individual_a - is_individual_b;
    if (result != 0)
        return result;
    return compare_perceived_things_by_z_order(a, b);
}

PerceivedThing find_perceived_individual_at(Thing observer, Coord location) {
    PerceivedThing individual;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&individual);)
        if (individual->thing_type == ThingType_INDIVIDUAL && individual->location == location)
            return individual;
    return nullptr;
}
void find_perceived_things_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing thing;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);)
        if (thing->location == location)
            output_sorted_list->append(thing);
    sort<PerceivedThing, compare_perceived_things_by_type_and_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void find_perceived_items_at(Thing observer, Coord location, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing thing;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);)
        if (thing->thing_type != ThingType_INDIVIDUAL && thing->location == location)
            output_sorted_list->append(thing);
    sort<PerceivedThing, compare_perceived_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
Thing find_individual_at(Coord location) {
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);) {
        if (individual->location.x == location.x && individual->location.y == location.y)
            return individual;
    }
    return nullptr;
}

void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = game->actual_things.value_iterator(); iterator.next(&item);)
        if (item->thing_type != ThingType_INDIVIDUAL && item->container_id == container_id)
            output_sorted_list->append(item);
    sort<Thing, compare_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void find_items_in_inventory(Thing observer, uint256 container_id, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing item;
    for (auto iterator = observer->life()->knowledge.perceived_things.value_iterator(); iterator.next(&item);)
        if (item->thing_type != ThingType_INDIVIDUAL && item->container_id == container_id)
            output_sorted_list->append(item);
    sort<PerceivedThing, compare_perceived_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void get_abilities(Thing individual, List<AbilityId> * output_sorted_abilities) {
    switch (individual->physical_species_id()) {
        case SpeciesId_HUMAN:
        case SpeciesId_OGRE:
        case SpeciesId_LICH:
        case SpeciesId_PINK_BLOB:
        case SpeciesId_AIR_ELEMENTAL:
        case SpeciesId_DOG:
        case SpeciesId_ANT:
        case SpeciesId_BEE:
        case SpeciesId_BEETLE:
        case SpeciesId_SCORPION:
        case SpeciesId_SNAKE:
            break;
        case SpeciesId_SHAPESHIFTER:
            output_sorted_abilities->append(AbilityId_ASSUME_FORM);
            break;
        case SpeciesId_TAR_ELEMENTAL:
            output_sorted_abilities->append(AbilityId_THROW_TAR);
            break;
        case SpeciesId_COBRA:
            output_sorted_abilities->append(AbilityId_SPIT_BLINDING_VENOM);
            break;

        case SpeciesId_COUNT:
        case SpeciesId_UNSEEN:
            unreachable();
    }

    switch (individual->life()->original_species_id) {
        case SpeciesId_HUMAN:
        case SpeciesId_OGRE:
        case SpeciesId_LICH:
        case SpeciesId_PINK_BLOB:
        case SpeciesId_AIR_ELEMENTAL:
        case SpeciesId_TAR_ELEMENTAL:
        case SpeciesId_DOG:
        case SpeciesId_ANT:
        case SpeciesId_BEE:
        case SpeciesId_BEETLE:
        case SpeciesId_SCORPION:
        case SpeciesId_SNAKE:
        case SpeciesId_COBRA:
            break;
        case SpeciesId_SHAPESHIFTER:
            if (output_sorted_abilities->index_of(AbilityId_ASSUME_FORM) == -1)
                output_sorted_abilities->append(AbilityId_ASSUME_FORM);
            break;

        case SpeciesId_COUNT:
        case SpeciesId_UNSEEN:
            unreachable();
    }
}
bool is_ability_ready(Thing actor, AbilityId ability_id) {
    const List<AbilityCooldown> & ability_cooldowns = actor->ability_cooldowns;
    for (int i = 0; i < ability_cooldowns.length(); i++)
        if (ability_cooldowns[i].ability_id == ability_id)
            return false;
    return true;
}

void find_items_on_floor(Coord location, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = game->actual_things.value_iterator(); iterator.next(&item);)
        if (item->thing_type != ThingType_INDIVIDUAL && item->location == location)
            output_sorted_list->append(item);
    sort<Thing, compare_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}

static void do_move(Thing mover, Coord new_position) {
    Coord old_position = mover->location;
    mover->location = new_position;

    compute_vision(mover);

    // notify other individuals who could see that move
    publish_event(Event::move(mover, old_position));

    if (mover->physical_species()->sucks_up_items) {
        // pick up items for free
        List<Thing> floor_items;
        find_items_on_floor(new_position, &floor_items);
        for (int i = 0; i < floor_items.length(); i++) {
            pickup_item(mover, floor_items[i]);
            publish_event(Event::individual_sucks_up_item(mover->id, floor_items[i]->id));
        }
    }
}
void attempt_move(Thing actor, Coord new_position) {
    if (!is_open_space(game->actual_map_tiles[new_position])) {
        publish_event(Event::bump_into_location(actor, new_position, false));
        return;
    }
    Thing target = find_individual_at(new_position);
    if (target != nullptr) {
        // this is not attacking
        publish_event(Event::bump_into_individual(actor, target));
        return;
    }
    // clear to move
    do_move(actor, new_position);
}
// return iff expired and removed
bool check_for_status_expired(Thing individual, int index) {
    StatusEffect status_effect = individual->status_effects[index];
    if (status_effect.expiration_time > game->time_counter)
        return false;
    assert(status_effect.expiration_time == game->time_counter);
    switch (status_effect.type) {
        case StatusEffect::CONFUSION:
        case StatusEffect::SPEED:
        case StatusEffect::ETHEREAL_VISION:
        case StatusEffect::COGNISCOPY:
        case StatusEffect::BLINDNESS:
        case StatusEffect::POISON:
        case StatusEffect::INVISIBILITY:
        case StatusEffect::SLOWING:
            publish_event(Event::lose_status(individual->id, status_effect.type));
            break;
        case StatusEffect::POLYMORPH:
            // we've got a function for removing this status
            polymorph_individual(individual, individual->life()->original_species_id, 0);
            return true;

        case StatusEffect::COUNT:
            unreachable();
    }
    individual->status_effects.swap_remove(index);
    switch (status_effect.type) {
        case StatusEffect::CONFUSION:
            break;
        case StatusEffect::SPEED:
        case StatusEffect::SLOWING:
            break;
        case StatusEffect::ETHEREAL_VISION:
            compute_vision(individual);
            break;
        case StatusEffect::COGNISCOPY:
            compute_vision(individual);
            break;
        case StatusEffect::BLINDNESS:
            compute_vision(individual);
            break;
        case StatusEffect::POISON:
            reset_hp_regen_timeout(individual);
            break;
        case StatusEffect::INVISIBILITY:
            break;
        case StatusEffect::POLYMORPH:
            compute_vision(individual);
            break;

        case StatusEffect::COUNT:
            unreachable();
    }
    return true;
}

static void cheatcode_kill(uint256 individual_id) {
    Thing individual = game->actual_things.get(individual_id);
    kill_individual(individual, nullptr, false);
}
void polymorph_individual(Thing individual, SpeciesId species_id, int duration) {
    int old_max_hitpoints = individual->max_hitpoints();

    int index = find_status(individual->status_effects, StatusEffect::POLYMORPH);
    if (species_id == individual->life()->original_species_id) {
        // undo any polymorph
        if (index == -1)
            return; // polymorphing into what you are does nothing

        publish_event(Event::polymorph(individual, species_id));
        individual->status_effects.swap_remove(index);
    } else {
        // add a update/polymorph status
        if (index == -1 || individual->status_effects[index].species_id != species_id)
            publish_event(Event::polymorph(individual, species_id));

        StatusEffect * polymorph_effect = find_or_put_status(individual, StatusEffect::POLYMORPH);
        polymorph_effect->expiration_time = game->time_counter + duration;
        if (polymorph_effect->species_id == species_id)
            return; // already polymorphed into that.
        polymorph_effect->species_id = species_id;
    }

    int new_max_hitpoints = individual->max_hitpoints();
    Life * life = individual->life();
    life->hitpoints = life->hitpoints * new_max_hitpoints / old_max_hitpoints;
    compute_vision(individual);
}
void cheatcode_spectate(Coord location) {
    cheatcode_spectator = find_individual_at(location);
}

bool can_move(Thing actor) {
    return actor->life()->last_movement_time + get_movement_cost(actor) <= game->time_counter;
}
static bool can_act(Thing actor) {
    return actor->life()->last_action_time + action_cost <= game->time_counter;
}
static bool is_direction(Coord direction, bool allow_self) {
    // has to be made of 0's, 1's, and -1's, but not all 0's
    if (direction == Coord{0, 0})
        return allow_self;
    if (!(direction.x == -1 || direction.x == 0 || direction.x == 1))
        return false;
    if (!(direction.y == -1 || direction.y == 0 || direction.y == 1))
        return false;
    return true;
}
static bool is_in_my_inventory(Thing actor, uint256 item_id) {
    PerceivedThing item = actor->life()->knowledge.perceived_things.get(item_id, nullptr);
    if (item == nullptr)
        return false;
    return item->container_id == actor->id;
}
bool validate_action(Thing actor, const Action & action) {
    switch (action.id) {
        case Action::WAIT:
            // always an option
            return true;
        case Action::MOVE:
        case Action::ATTACK:
            // you can try to move/attack any direction
            return is_direction(action.coord(), false);
        case Action::PICKUP: {
            PerceivedThing item = actor->life()->knowledge.perceived_things.get(action.item(), nullptr);
            if (item == nullptr)
                return false;
            if (item->location != actor->location)
                return false;
            return true;
        }
        case Action::DROP:
        case Action::QUAFF:
            return is_in_my_inventory(actor, action.item());
        case Action::THROW: {
            const Action::CoordAndItem & data = action.coord_and_item();
            if (!is_direction(data.coord, false))
                return false;
            if (!is_in_my_inventory(actor, data.item))
                return false;
            return true;
        }
        case Action::ZAP: {
            const Action::CoordAndItem & data = action.coord_and_item();
            if (!is_direction(data.coord, true))
                return false;
            PerceivedThing item = actor->life()->knowledge.perceived_things.get(data.item, nullptr);
            if (item == nullptr)
                return false;
            if (item->container_id != actor->id)
                return false;
            if (item->thing_type != ThingType_WAND)
                return false;
            return true;
        }
        case Action::READ_BOOK: {
            const Action::CoordAndItem & data = action.coord_and_item();
            if (!is_direction(data.coord, true))
                return false;
            PerceivedThing item = actor->life()->knowledge.perceived_things.get(data.item, nullptr);
            if (item == nullptr)
                return false;
            if (item->container_id != actor->id)
                return false;
            if (item->thing_type != ThingType_BOOK)
                return false;
            return true;
        }
        case Action::GO_DOWN:
            return actor->life()->knowledge.tiles[actor->location] == TileType_STAIRS_DOWN;
        case Action::ABILITY: {
            const Action::AbilityData & data = action.ability();
            List<AbilityId> valid_abilities;
            get_abilities(actor, &valid_abilities);
            if (valid_abilities.index_of(data.ability_id) == -1)
                return false;
            if (!is_ability_ready(actor, data.ability_id))
                return false;
            return true;
        }

        case Action::CHEATCODE_HEALTH_BOOST:
        case Action::CHEATCODE_POLYMORPH:
        case Action::CHEATCODE_IDENTIFY:
        case Action::CHEATCODE_GO_DOWN:
        case Action::CHEATCODE_GAIN_LEVEL:
            if (actor != player_actor())
                return false;
            return true;
        case Action::CHEATCODE_KILL:
            if (actor != player_actor())
                return false;
            if (game->actual_things.get(action.item()) == nullptr)
                return false;
            return true;
        case Action::CHEATCODE_WISH:
            if (actor != player_actor())
                return false;
            if (action.thing().thing_type == ThingType_INDIVIDUAL)
                return false;
            return true;
        case Action::CHEATCODE_GENERATE_MONSTER: {
            if (actor != player_actor())
                return false;
            const Action::GenerateMonster & data = action.generate_monster();
            Coord location = data.location;
            if (!(is_in_bounds(location) && is_open_space(game->actual_map_tiles[location])))
                return false;
            if (find_individual_at(location) != nullptr)
                return false;
            return true;
        }

        case Action::COUNT:
        case Action::UNDECIDED:
        case Action::AUTO_WAIT:
            unreachable();
    }
    unreachable();
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
    if (direction == Coord{0, 0})
        return direction; // can't get that wrong
    if (has_status(individual, StatusEffect::CONFUSION) && random_int(2, "is_direction_confused") == 0) {
        // which direction are we attempting
        for (int i = 0; i < 8; i++) {
            if (directions_by_rotation[i] != direction)
                continue;
            // turn left or right 45 degrees
            i = euclidean_mod(i + 2 * random_int(2, "confused_direction") - 1, 8);
            return directions_by_rotation[i];
        }
        panic("direction not found");
    }
    return direction; // not confused
}

// return true if time should pass, false if we used an instantaneous cheatcode.
static bool take_action(Thing actor, const Action & action) {
    assert(validate_action(actor, action));
    // we know you can attempt the action, but it won't necessarily turn out the way you expected it.

    switch (action.id) {
        case Action::WAIT:
            break;
        case Action::MOVE: {
            Coord new_position = actor->location + confuse_direction(actor, action.coord());
            attempt_move(actor, new_position);
            break;
        }
        case Action::ATTACK: {
            Coord new_position = actor->location + confuse_direction(actor, action.coord());
            Thing target = find_individual_at(new_position);
            if (target != nullptr) {
                attack(actor, target);
                break;
            } else {
                bool is_air = is_open_space(game->actual_map_tiles[new_position]);
                publish_event(Event::attack_location(actor, new_position, is_air));
                break;
            }
        }
        case Action::ZAP:
            zap_wand(actor, action.coord_and_item().item, confuse_direction(actor, action.coord_and_item().coord));
            break;
        case Action::READ_BOOK:
            read_book(actor, action.coord_and_item().item, confuse_direction(actor, action.coord_and_item().coord));
            break;
        case Action::PICKUP:
            pickup_item(actor, game->actual_things.get(action.item()));
            publish_event(Event::individual_picks_up_item(actor->id, action.item()));
            break;
        case Action::DROP:
            drop_item_to_the_floor(game->actual_things.get(action.item()), actor->location);
            break;
        case Action::QUAFF: {
            Thing item = game->actual_things.get(action.item());
            use_potion(actor, actor, item, false);
            break;
        }
        case Action::THROW:
            throw_item(actor, game->actual_things.get(action.coord_and_item().item), confuse_direction(actor, action.coord_and_item().coord));
            break;

        case Action::GO_DOWN:
            go_down();
            break;
        case Action::ABILITY: {
            const Action::AbilityData & data = action.ability();
            do_ability(actor, data.ability_id, confuse_direction(actor, data.direction));
            break;
        }

        case Action::CHEATCODE_HEALTH_BOOST:
            actor->life()->hitpoints += 100;
            return false;
        case Action::CHEATCODE_KILL:
            cheatcode_kill(action.item());
            return false;
        case Action::CHEATCODE_POLYMORPH:
            polymorph_individual(player_actor(), action.species(), 1000000);
            // this one does take time, because your movement cost may have changed
            return false;
        case Action::CHEATCODE_GENERATE_MONSTER: {
            const Action::GenerateMonster & data = action.generate_monster();
            spawn_a_monster(data.species, data.decision_maker, data.location);
            return false;
        }
        case Action::CHEATCODE_WISH: {
            const Action::Thing & data = action.thing();
            switch (data.thing_type) {
                case ThingType_INDIVIDUAL:
                    unreachable();
                case ThingType_WAND:
                    drop_item_to_the_floor(create_wand(data.wand_id), actor->location);
                    return false;
                case ThingType_POTION:
                    drop_item_to_the_floor(create_potion(data.potion_id), actor->location);
                    return false;
                case ThingType_BOOK:
                    drop_item_to_the_floor(create_book(data.book_id), actor->location);
                    return false;

                case ThingType_COUNT:
                    unreachable();
            }
            unreachable();
        }
        case Action::CHEATCODE_IDENTIFY:
            for (int i = 0; i < WandId_COUNT; i++)
                player_actor()->life()->knowledge.wand_identities[game->actual_wand_descriptions[i]] = (WandId)i;
            for (int i = 0; i < PotionId_COUNT; i++)
                player_actor()->life()->knowledge.potion_identities[game->actual_potion_descriptions[i]] = (PotionId)i;
            for (int i = 0; i < BookId_COUNT; i++)
                player_actor()->life()->knowledge.book_identities[game->actual_book_descriptions[i]] = (BookId)i;
            return false;
        case Action::CHEATCODE_GO_DOWN:
            if (game->dungeon_level < final_dungeon_level)
                go_down();
            break;
        case Action::CHEATCODE_GAIN_LEVEL:
            level_up(actor, true);
            return false;

        case Action::COUNT:
        case Action::UNDECIDED:
        case Action::AUTO_WAIT:
            unreachable();
    }

    // now pay for the action
    Life * life = actor->life();
    int movement_cost = get_movement_cost(actor);
    if (action.id == Action::WAIT) {
        if (can_move(actor) && can_act(actor)) {
            int lowest_cost = min(movement_cost, action_cost);
            life->last_movement_time = game->time_counter + lowest_cost - movement_cost;
            life->last_action_time = game->time_counter + lowest_cost - action_cost;
        } else if (can_move(actor)) {
            int lowest_cost = min(movement_cost, action_cost - (int)(game->time_counter - life->last_action_time));
            life->last_movement_time += lowest_cost;
        } else {
            // can act
            int lowest_cost = min(action_cost, movement_cost - (int)(game->time_counter - life->last_movement_time));
            life->last_action_time += lowest_cost;
        }
    } else if (action.id == Action::MOVE) {
        life->last_movement_time = game->time_counter;
        if (movement_cost == action_cost) {
            life->last_action_time = game->time_counter;
        } else if (movement_cost > action_cost) {
            // slow individual.
            // wait action_cost until you can act, and even longer before you can move again.
            life->last_action_time = game->time_counter;
        } else {
            // fast individual
            if (life->last_action_time < life->last_movement_time + movement_cost - action_cost)
                life->last_action_time = life->last_movement_time + movement_cost - action_cost;
        }
    } else {
        if (!can_act(actor))
            panic("can't act");
        // action
        life->last_action_time = game->time_counter;
        // you have to wait for the action before you can move again.
        if (life->last_movement_time < life->last_action_time + action_cost - movement_cost)
            life->last_movement_time = life->last_action_time + action_cost - movement_cost;
    }

    return true;
}

// advance time for an individual
static void age_individual(Thing individual) {
    if (!game->test_mode) {
        regen_hp(individual);
        regen_mp(individual);
    }

    if (individual->physical_species()->auto_throws_items) {
        // there's a chance per item they're carrying
        List<Thing> inventory;
        find_items_in_inventory(individual->id, &inventory);
        for (int i = 0; i < inventory.length(); i++) {
            if (!game->test_mode && random_int(100, nullptr) == 0) {
                // throw item in random direction
                throw_item(individual, inventory[i], directions[random_int(8, nullptr)]);
            }
        }
    }

    for (int i = 0; i < individual->status_effects.length(); i++) {
        if (check_for_status_expired(individual, i))
            i--;
    }

    for (int i = 0; i < individual->ability_cooldowns.length(); i++) {
        assert(individual->ability_cooldowns[i].expiration_time >= game->time_counter);
        if (individual->ability_cooldowns[i].expiration_time == game->time_counter) {
            individual->ability_cooldowns.swap_remove(i);
            i--;
        }
    }

    List<RememberedEvent> & remembered_events = individual->life()->knowledge.remembered_events;
    if (remembered_events.length() >= 1000) {
        remembered_events.remove_range(0, 500);
        individual->life()->knowledge.event_forget_counter++;
    }

    // clean up stale placeholders
    List<uint256> delete_ids;
    PerceivedThing thing;
    for (auto iterator = individual->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
        if (thing->thing_type == ThingType_INDIVIDUAL && thing->is_placeholder) {
            if (game->time_counter - thing->last_seen_time >= 12 * 20) {
                delete_ids.append(thing->id);
                List<PerceivedThing> inventory;
                find_items_in_inventory(individual, thing->id, &inventory);
                for (int i = 0; i < inventory.length(); i++)
                    delete_ids.append(inventory[i]->id);
            }
        }
    }
    for (int i = 0; i < delete_ids.length(); i++)
        individual->life()->knowledge.perceived_things.remove(delete_ids[i]);
}

static void delete_dead_things() {
    List<Thing> delete_things;
    {
        Thing thing;
        for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);)
            if (!thing->still_exists && thing->id != game->you_id)
                delete_things.append(thing);
    }
    // tell everyone to forget about this stuff.
    for (int i = 0; i < delete_things.length(); i++)
        publish_event(Event::delete_thing(delete_things[i]->id));

    // and actually delete it all
    List<uint256> fix_z_order_container_ids;
    for (int i = 0; i < delete_things.length(); i++) {
        Thing thing = delete_things[i];
        game->actual_things.remove(thing->id);
        if (thing->container_id != uint256::zero())
            if (fix_z_order_container_ids.index_of(thing->container_id) == -1)
                fix_z_order_container_ids.append(thing->container_id);
    }
    for (int i = 0; i < fix_z_order_container_ids.length(); i++)
        fix_z_orders(fix_z_order_container_ids[i]);
}

List<Thing> poised_individuals;
int poised_individuals_index = 0;
// this function will return only when we're expecting player input
void run_the_game() {
    while (you()->still_exists) {
        if (poised_individuals.length() == 0) {
            game->time_counter++;

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
                if (!individual->still_exists)
                    continue;
                age_individual(individual);
                if (can_act(individual)) {
                    poised_individuals.append(individual);
                    // log the passage of time in the message window.
                    // this actually only observers time in increments of your movement cost
                    List<RememberedEvent> & events = individual->life()->knowledge.remembered_events;
                    if (events.length() > 0 && events[events.length() - 1] != nullptr)
                        events.append(nullptr);
                }
            }
        }

        // move individuals
        for (; poised_individuals_index < poised_individuals.length(); poised_individuals_index++) {
            Thing individual = poised_individuals[poised_individuals_index];
            bool should_time_pass = false;
            while (!should_time_pass) {
                if (!individual->still_exists)
                    break; // sorry, buddy. you were that close to making another move.
                Action action = decision_makers[individual->life()->decision_maker](individual);
                if (action.id == Action::UNDECIDED) {
                    // give the player some time to think.
                    // we'll resume right back where we left off.
                    return;
                }
                should_time_pass = take_action(individual, action);

                assert(game->observer_to_active_identifiable_item.size() == 0);
            }
        }
        poised_individuals.clear();
        poised_individuals_index = 0;

        delete_dead_things();
    }
    delete_save_file();
}

void change_map(Coord location, TileType new_tile_type) {
    game->actual_map_tiles[location] = new_tile_type;
    // recompute everyone's vision
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);)
        compute_vision(individual);
}

void fix_perceived_z_orders(Thing observer, uint256 container_id) {
    List<PerceivedThing> inventory;
    find_items_in_inventory(observer, container_id, &inventory);
    for (int i = 0; i < inventory.length(); i++)
        inventory[i]->z_order = i;
}
// it's ok to call this with a bogus/deleted id
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
