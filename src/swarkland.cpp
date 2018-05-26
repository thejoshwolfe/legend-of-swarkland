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
        publish_event(Event::create(Event::MELEE_KILL, attacker->id, individual->id));
    } else {
        publish_event(Event::create(Event::DIE, individual->id));
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
        publish_event(Event::create(Event::LEVEL_UP, individual->id));
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
    publish_event(Event::create(Event::GAIN_STATUS, target->id, StatusEffect::POISON));

    StatusEffect * poison = find_or_put_status(target, StatusEffect::POISON, game->time_counter + random_midpoint(600, "poison_expiriration"));
    poison->who_is_responsible = attacker->id;
    poison->poison_next_damage_time = game->time_counter + 12 * 3;
}
void blind_individual(Thing attacker, Thing target, int expiration_midpoint) {
    publish_event(Event::create(Event::GAIN_STATUS, target->id, StatusEffect::BLINDNESS));
    StatusEffect * effect = find_or_put_status(target, StatusEffect::BLINDNESS, game->time_counter + random_midpoint(expiration_midpoint, "blindness_expiration"));
    effect->who_is_responsible = attacker->id;
    compute_vision(target);
}
void slow_individual(Thing actor, Thing target) {
    StatusEffect * effect = find_or_put_status(target, StatusEffect::SLOWING, game->time_counter + random_midpoint(200, "slowing_duration"));
    effect->who_is_responsible = actor->id;
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

    publish_event(Event::create(Event::ITEM_DROPS_TO_THE_FLOOR, item->id, item->location));

    Thing individual = find_individual_at(location);
    if (individual != nullptr && individual->physical_species()->sucks_up_items) {
        // suck it
        pickup_item(individual, item);
        publish_event(Event::create(Event::INDIVIDUAL_SUCKS_UP_ITEM, individual->id, item->id));
    }
}

// normal melee attack
static void attack(Thing attacker, Thing target, int bonus_damage) {
    if (attempt_dodge(attacker, target)) {
        publish_event(Event::create(Event::DODGE_ATTACK, attacker->id, target->id));
        return;
    }
    // it's a hit
    publish_event(Event::create(Event::ATTACK_INDIVIDUAL, attacker->id, target->id));
    int attack_power = attacker->innate_attack_power();
    Thing weapon = get_equipped_weapon(attacker);
    if (weapon != nullptr)
        attack_power += get_weapon_damage(weapon);
    attack_power += bonus_damage;
    int min_damage = (attack_power + 1) / 2;
    int damage = random_inclusive(min_damage, min_damage + attack_power / 2, "melee_damage");
    damage_individual(target, damage, attacker, true);
    reset_hp_regen_timeout(attacker);
    if (target->still_exists && attacker->physical_species()->poison_attack && random_int(4, "poison_attack") == 0)
        poison_individual(attacker, target);

    // newton's 3rd
    Coord attack_vector = target->location - attacker->location;
    apply_impulse(target, attack_vector);
    apply_impulse(attacker, -attack_vector);
}

static void attack_location(Thing actor, Coord location, int bonus_damage) {
    Thing target = find_individual_at(location);
    if (target != nullptr) {
        attack(actor, target, bonus_damage);
    } else {
        bool is_air = is_open_space(game->actual_map_tiles[location]);
        publish_event(Event::create(Event::ATTACK_LOCATION, actor->id, location, is_air));
    }
}

static void do_lunge_attack(Thing actor, Coord direction) {
    assert(lunge_attack_range == 2);
    // move at least once
    if (!attempt_move(actor, actor->location + direction, actor))
        return; // bonk!
    int bonus_damage;
    if (find_perceived_individual_at(actor, actor->location + direction) != nullptr) {
        // short range attack
        bonus_damage = 7;
    } else {
        // move one more before attacking
        if (!attempt_move(actor, actor->location + direction, actor))
            return; // bonk!
        // long range attack
        bonus_damage = 5;
    }
    // we made it to the end of the range. now attack.
    attack_location(actor, actor->location + direction, bonus_damage);
}

static void do_ability(Thing actor, AbilityId ability_id, Coord direction) {
    int cooldown_time;
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            publish_event(Event::create(Event::SPIT_BLINDING_VENOM, actor->id));
            cooldown_time = random_midpoint(150, "spit_blinding_venom_cooldown");
            break;
        case AbilityId_THROW_TAR:
            publish_event(Event::create(Event::THROW_TAR, actor->id));
            cooldown_time = random_midpoint(150, "throw_tar_cooldown");
            break;
        case AbilityId_ASSUME_FORM:
            cooldown_time = random_midpoint(48, "assume_form_cooldown");
            break;
        case AbilityId_LUNGE_ATTACK:
            publish_event(Event::create(Event::LUNGE, actor->id));
            cooldown_time = 48;
            break;
        case AbilityId_COUNT:
            unreachable();
    }
    if (cooldown_time > 0)
        actor->ability_cooldowns.append(AbilityCooldown{ability_id, game->time_counter + cooldown_time});

    Coord range_window = get_ability_range_window(ability_id);
    int range;
    switch (ability_id) {
        case AbilityId_SPIT_BLINDING_VENOM:
            range = random_inclusive(range_window.x, range_window.y, "spit_blinding_venom_distance");
            break;
        case AbilityId_THROW_TAR:
            range = random_inclusive(range_window.x, range_window.y, "throw_tar_distance");
            break;
        case AbilityId_ASSUME_FORM:
            range = random_inclusive(range_window.x, range_window.y, "assume_form_distance");
            break;
        case AbilityId_LUNGE_ATTACK:
            do_lunge_attack(actor, direction);
            return;
        case AbilityId_COUNT:
            unreachable();
    }

    Coord cursor = actor->location;
    for (int i = 0; i < range; i++) {
        cursor += direction;
        Thing target = find_individual_at(cursor);
        if (target != nullptr) {
            switch (ability_id) {
                case AbilityId_SPIT_BLINDING_VENOM:
                    publish_event(Event::create(Event::BLINDING_VENOM_HIT_INDIVIDUAL, target->id));
                    blind_individual(actor, target, 600);
                    return;
                case AbilityId_THROW_TAR:
                    publish_event(Event::create(Event::TAR_HIT_INDIVIDUAL, target->id));
                    publish_event(Event::create(Event::GAIN_STATUS, target->id, StatusEffect::SLOWING));
                    slow_individual(actor, target);
                    return;
                case AbilityId_ASSUME_FORM:
                    polymorph_individual(actor, target->physical_species_id(), random_midpoint(240, "ability_assume_form_duration"));
                    return;
                case AbilityId_LUNGE_ATTACK:
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
    publish_event(Event::create(Event::APPEAR, individual->id));
    return individual;
}

static void spawn_a_monster_with_equipment() {
    Thing individual = spawn_a_monster(SpeciesId_COUNT, DecisionMakerType_AI, Coord::nowhere());

    WeaponId weapon_id;
    switch (individual->life()->original_species_id) {
        case SpeciesId_OGRE:
            if (random_int(3, nullptr) == 0) return;
            weapon_id = WeaponId_BATTLEAXE;
            break;
        case SpeciesId_SHAPESHIFTER:
            if (random_int(3, nullptr) == 0) return;
            weapon_id = WeaponId_DAGGER;
            break;
        case SpeciesId_HUMAN:
        case SpeciesId_LICH:
        case SpeciesId_PINK_BLOB:
        case SpeciesId_AIR_ELEMENTAL:
        case SpeciesId_DOG:
        case SpeciesId_ANT:
        case SpeciesId_BEE:
        case SpeciesId_BEETLE:
        case SpeciesId_SCORPION:
        case SpeciesId_SNAKE:
        case SpeciesId_TAR_ELEMENTAL:
        case SpeciesId_COBRA:
            // no starting equipment
            return;

        case SpeciesId_COUNT:
        case SpeciesId_UNSEEN:
            unreachable();
    }

    pickup_item(individual, create_weapon(weapon_id));
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
        spawn_a_monster_with_equipment();
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
    bool do_regen = true;
    int index;
    if ((index = find_status(individual->status_effects, StatusEffect::POISON)) != -1) {
        // you're poisoned
        do_regen = immune_to_poison(individual);
        StatusEffect * poison = &individual->status_effects[index];
        if (poison->poison_next_damage_time == game->time_counter) {
            // the poison does battle with your metabolism!
            if (immune_to_poison(individual)) {
                // reduce the poison time
                poison->expiration_time -= random_midpoint(15 * 12, "speedy_poison_recovery");
                if (poison->expiration_time < game->time_counter)
                    poison->expiration_time = game->time_counter;
                // this will usually be unobservable, but if your scorpion polymorph runs out,
                // then you might have some poison left to deal with.
            } else {
                // ouch
                Thing attacker = game->actual_things.get(poison->who_is_responsible, nullptr);
                if (attacker != nullptr && !attacker->still_exists)
                    attacker = nullptr;
                damage_individual(individual, 1, attacker, false);
            }
            poison->poison_next_damage_time = game->time_counter + random_midpoint(7 * 12, "poison_damage");
        }
    }
    if (do_regen && life->hp_regen_deadline == game->time_counter) {
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

static int compare_things_by_z_order(const Thing & a, const Thing & b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
static int compare_perceived_things_by_z_order(const PerceivedThing & a, const PerceivedThing & b) {
    return a->z_order < b->z_order ? -1 : a->z_order > b->z_order ? 1 : 0;
}
int compare_perceived_things_by_type_and_z_order(const PerceivedThing & a, const PerceivedThing & b) {
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

Thing get_equipped_weapon(Thing individual) {
    List<Thing> inventory;
    find_items_in_inventory(individual->id, &inventory);
    Thing best_weapon = nullptr;
    int best_weapon_damage = 0;
    for (int i = 0; i < inventory.length(); i++) {
        Thing item = inventory[i];
        if (item->thing_type != ThingType_WEAPON)
            continue;
        int item_damage = get_weapon_damage(item);
        if (item_damage > best_weapon_damage) {
            best_weapon = item;
            best_weapon_damage = item_damage;
        }
    }
    return best_weapon;
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
            break;
        case SpeciesId_SHAPESHIFTER:
            output_sorted_abilities->append(AbilityId_ASSUME_FORM);
            break;
        case SpeciesId_TAR_ELEMENTAL:
            output_sorted_abilities->append(AbilityId_THROW_TAR);
            break;
        case SpeciesId_COBRA:
            output_sorted_abilities->append(AbilityId_SPIT_BLINDING_VENOM);
            output_sorted_abilities->append(AbilityId_LUNGE_ATTACK);
            break;
        case SpeciesId_SNAKE:
            output_sorted_abilities->append(AbilityId_LUNGE_ATTACK);
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

void suck_up_item(Thing actor, Thing item) {
    pickup_item(actor, item);
    publish_event(Event::create(Event::INDIVIDUAL_SUCKS_UP_ITEM, actor->id, item->id));
}

static bool can_propel_self_while_levitating(Thing actor) {
    if (actor->physical_species()->flying)
        return true;
    if (is_solid_wall_within_reach(actor->location))
        return true;
    return false;
}
static bool can_propel_self(Thing actor) {
    if (!has_status(actor, StatusEffect::LEVITATING))
        return true;
    return can_propel_self_while_levitating(actor);
}
bool is_touching_ground(Thing individual) {
    if (individual->physical_species()->flying) return false;
    if (has_status(individual, StatusEffect::LEVITATING)) return false;
    return true;
}

static void do_move(Thing mover, Coord new_position, Thing who_is_responsible) {
    Coord old_position = mover->location;
    mover->location = new_position;

    compute_vision(mover);

    // notify other individuals who could see that move
    publish_event(Event::create(Event::MOVE, mover->id, old_position, mover->location));

    int index;
    if ((index = find_status(mover->status_effects, StatusEffect::LEVITATING)) != -1) {
        Coord momentum;
        if (can_propel_self_while_levitating(mover)) {
            // steady...
            momentum = Coord{0, 0};
        } else {
            // we're going this way
            momentum = new_position - old_position;
        }
        mover->status_effects[index].coord = momentum;
    }

    if (mover->physical_species()->sucks_up_items) {
        // pick up items for free
        List<Thing> floor_items;
        find_items_on_floor(new_position, &floor_items);
        for (int i = 0; i < floor_items.length(); i++)
            suck_up_item(mover, floor_items[i]);
    }

    // after any actual movement, your movement timer starts over.
    // this makes force pushing enemies into lava do more predictable damage.
    // note that you can still act at the same time you would have been able to, just not move.
    mover->life()->last_movement_time = game->time_counter;

    if (mover->id != who_is_responsible->id)
        find_or_put_status(mover, StatusEffect::PUSHED, game->time_counter + 10 * 12)->who_is_responsible = who_is_responsible->id;
}
// return if it worked, otherwise bumped into something.
bool attempt_move(Thing actor, Coord new_position, Thing who_is_responsible) {
    if (!is_open_space(game->actual_map_tiles[new_position])) {
        // moving into a wall
        if (has_status(actor, StatusEffect::BURROWING)) {
            // maybe we can tunnel through this.
            if (is_diggable_wall(game->actual_map_tiles[new_position])) {
                // burrow through the wall.
                change_map(new_position, TileType_DIRT_FLOOR);
                do_move(actor, new_position, who_is_responsible);
                publish_event(Event::create(Event::INDIVIDUAL_BURROWS_THROUGH_WALL, actor->id, new_position, true));
                return true;
            }
        }
        publish_event(Event::create(Event::BUMP_INTO_LOCATION, actor->id, new_position, false));
        return false;
    }
    Thing target = find_individual_at(new_position);
    if (target != nullptr) {
        // this is not attacking
        publish_event(Event::create(Event::BUMP_INTO_INDIVIDUAL, actor->id, target->id));
        // newton's 3rd
        Coord collisison_vector = target->location - actor->location;
        apply_impulse(target, collisison_vector);
        apply_impulse(actor, -collisison_vector);
        return false;
    }
    // clear to move
    do_move(actor, new_position, who_is_responsible);
    return true;
}
bool attempt_dodge(Thing attacker, Thing target) {
    Life * life = target->life();
    if (!life->can_dodge || !can_see_thing(target, attacker->id))
        return false;
    // can dodge
    int numerator = 2;
    int denominator = get_movement_cost(target);
    bool success = denominator < numerator || random_int(denominator, "dodge") < numerator;
    return success;
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
        case StatusEffect::BURROWING:
        case StatusEffect::LEVITATING:
            publish_event(Event::create(Event::LOSE_STATUS, individual->id, status_effect.type));
            break;
        case StatusEffect::POLYMORPH:
            // we've got a function for removing this status
            polymorph_individual(individual, individual->life()->original_species_id, 0);
            return true;
        case StatusEffect::PUSHED:
            break;

        case StatusEffect::COUNT:
            unreachable();
    }
    individual->status_effects.swap_remove(index);
    switch (status_effect.type) {
        case StatusEffect::CONFUSION:
        case StatusEffect::SPEED:
        case StatusEffect::SLOWING:
        case StatusEffect::INVISIBILITY:
        case StatusEffect::BURROWING:
        case StatusEffect::LEVITATING:
        case StatusEffect::PUSHED:
            break;
        case StatusEffect::ETHEREAL_VISION:
        case StatusEffect::COGNISCOPY:
        case StatusEffect::BLINDNESS:
            compute_vision(individual);
            break;
        case StatusEffect::POISON:
            reset_hp_regen_timeout(individual);
            break;

        case StatusEffect::POLYMORPH:
        case StatusEffect::COUNT:
            unreachable();
    }
    return true;
}
// impluse as in change in momentum.
void apply_impulse(Thing individual, Coord vector) {
    // only do this for individuals. for items, you do other things, like throw them.
    assert(individual->thing_type == ThingType_INDIVIDUAL);
    int index;
    if ((index = find_status(individual->status_effects, StatusEffect::LEVITATING)) != -1) {
        Coord & momentum = individual->status_effects[index].coord;
        if (can_propel_self_while_levitating(individual)) {
            // if you can steady yourself, you're immune to momentum
            assert(momentum == (Coord{0, 0}));
            return;
        }
        momentum = clamp(momentum + vector, Coord{-1, -1}, Coord{1, 1});
    }
}

static void cheatcode_kill(uint256 individual_id) {
    Thing individual = game->actual_things.get(individual_id);
    kill_individual(individual, nullptr, false);
}
void polymorph_individual(Thing individual, SpeciesId new_species_id, int duration) {
    int old_max_hitpoints = individual->max_hitpoints();
    int old_species_id = individual->physical_species_id();

    if (new_species_id == individual->life()->original_species_id) {
        // undo any polymorph
        int index = find_status(individual->status_effects, StatusEffect::POLYMORPH);
        if (index == -1)
            return; // polymorphing from your natural form to your natural form does nothing

        individual->status_effects.swap_remove(index);
        publish_event(Event::create(Event::POLYMORPH, individual->id, new_species_id));
    } else {
        // add or refresh polymorph status
        StatusEffect * polymorph_effect = find_or_put_status(individual, StatusEffect::POLYMORPH, game->time_counter + duration);
        polymorph_effect->species_id = new_species_id;

        if (old_species_id != new_species_id)
            publish_event(Event::create(Event::POLYMORPH, individual->id, new_species_id));
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
    if (action.id == Action::WAIT) {
        // we shouldn't even ask an individual for an action if they weren't allowed to wait.
        assert(can_act(actor) || can_move(actor));
    } else if (action.id == Action::MOVE) {
        if (!can_move(actor)) return false;
    } else {
        if (!can_act(actor)) return false;
    }
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

static Coord confuse_direction(Thing individual, Coord direction) {
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
    // we know you can attempt the action, but it won't necessarily turn out the way you expected.

    bool can_dodge = false;
    bool just_successfully_moved = false;
    switch (action.id) {
        case Action::WAIT:
            // this is false when we're floating at high speed after doing an action
            can_dodge = can_act(actor);
            break;
        case Action::MOVE: {
            if (can_propel_self(actor)) {
                Coord new_position = actor->location + confuse_direction(actor, action.coord());
                can_dodge = attempt_move(actor, new_position, actor);
                just_successfully_moved = true;
            } else {
                publish_event(Event::create(Event::INDIVIDUAL_FLOATS_UNCONTROLLABLY, actor->id));
            }
            break;
        }
        case Action::ATTACK: {
            Coord new_position = actor->location + confuse_direction(actor, action.coord());
            attack_location(actor, new_position, 0);
            break;
        }
        case Action::ZAP:
            zap_wand(actor, action.coord_and_item().item, confuse_direction(actor, action.coord_and_item().coord));
            break;
        case Action::READ_BOOK:
            read_book(actor, action.coord_and_item().item, confuse_direction(actor, action.coord_and_item().coord));
            break;
        case Action::PICKUP:
            pickup_item(actor, game->actual_things.get(action.item()));
            publish_event(Event::create(Event::INDIVIDUAL_PICKS_UP_ITEM, actor->id, action.item()));
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
        case Action::CHEATCODE_POLYMORPH: {
            Thing individual = player_actor();
            polymorph_individual(individual, action.species(), 1);
            individual->life()->original_species_id = action.species();
            int index = find_status(individual->status_effects, StatusEffect::POLYMORPH);
            individual->status_effects.swap_remove(index);
            return false;
        }
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
                case ThingType_WEAPON:
                    drop_item_to_the_floor(create_weapon(data.weapon_id), actor->location);
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

    // uncontrollable momentum
    bool floating_uncontrollably = false;
    if (!just_successfully_moved && can_move(actor)) {
        // when you first shove off a wall, we don't want this to give you a double boost.
        int index;
        if ((index = find_status(actor->status_effects, StatusEffect::LEVITATING)) != -1) {
            if (can_propel_self_while_levitating(actor)) {
                // not affected by levitation
                actor->status_effects[index].coord = Coord{0, 0};
            } else {
                Coord momentum = actor->status_effects[index].coord;
                if (momentum != Coord{0, 0}) {
                    // also keep sliding through the air.
                    attempt_move(actor, actor->location + momentum, actor);
                    floating_uncontrollably = true;
                }
            }
        }
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
        } else if (can_act(actor)) {
            int lowest_cost = min(action_cost, movement_cost - (int)(game->time_counter - life->last_movement_time));
            life->last_action_time += lowest_cost;
        } else {
            // already handled
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
        assert(can_act(actor));
        // action
        life->last_action_time = game->time_counter;
        if (!floating_uncontrollably) {
            // you have to wait for the action before you can move again.
            if (life->last_movement_time < game->time_counter + action_cost - movement_cost)
                life->last_movement_time = game->time_counter + action_cost - movement_cost;
        }
    }

    life->can_dodge = can_dodge;

    return true;
}

static constexpr StatusEffect::Id reasons_why_im_here[] = {
    // being pushed is the most obvious reason
    StatusEffect::PUSHED,
    // confusion can trick you into falling into lava
    StatusEffect::CONFUSION,
    // blindness can prevent you from seeing lava
    StatusEffect::BLINDNESS,
};
static Thing who_is_responsible_for_being_here(Thing thing) {
    for (int i = 0; i < get_array_length(reasons_why_im_here); i++) {
        int index;
        if ((index = find_status(thing->status_effects, reasons_why_im_here[i])) != -1) {
            assert(thing->status_effects[index].who_is_responsible != uint256::zero());
            if (thing->status_effects[index].who_is_responsible == thing->id)
                continue;
            Thing who_is_responsible = game->actual_things.get(thing->status_effects[index].who_is_responsible, nullptr);
            if (who_is_responsible != nullptr && who_is_responsible->still_exists)
                return who_is_responsible;
            // we found the cause, but that person is dead. so no one gets credit.
            return nullptr;
        }
    }
    return nullptr;
}

static void age_things() {
    List<Thing> things_in_order;
    {
        Thing thing;
        for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);) {
            assert(thing->still_exists);
            things_in_order.append(thing);
        }
    }

    sort<Thing, compare_things_by_location>(things_in_order.raw(), things_in_order.length());

    // environment hazards
    for (int i = 0; i < things_in_order.length(); i++) {
        Thing thing = things_in_order[i];
        if (thing->location == Coord::nowhere())
            continue;
        switch (game->actual_map_tiles[thing->location]) {
            case TileType_DIRT_FLOOR:
            case TileType_MARBLE_FLOOR:
            case TileType_STAIRS_DOWN:
                break;
            case TileType_LAVA_FLOOR: {
                // damage over time
                int lava_damage = random_int(2, "lava_damage");
                if (lava_damage > 0) {
                    bool item_disintegrates = false;
                    switch (thing->thing_type) {
                        case ThingType_INDIVIDUAL:
                            if (is_touching_ground(thing)) {
                                publish_event(Event::create(Event::SEARED_BY_LAVA, thing->id));
                                damage_individual(thing, lava_damage, who_is_responsible_for_being_here(thing), false);
                            }
                            break;
                        case ThingType_WAND:
                        case ThingType_BOOK:
                        case ThingType_WEAPON:
                            item_disintegrates = true;
                            break;
                        case ThingType_POTION:
                            item_disintegrates = thing->potion_info()->potion_id != PotionId_POTION_OF_LEVITATION;
                            break;
                        case ThingType_COUNT:
                            unreachable();
                    }
                    if (item_disintegrates) {
                        publish_event(Event::create(Event::ITEM_DISINTEGRATES_IN_LAVA, thing->id, thing->location));
                        thing->still_exists = false;
                    }
                }
                break;
            }
            case TileType_BROWN_BRICK_WALL:
            case TileType_GRAY_BRICK_WALL:
            case TileType_BORDER_WALL:
                // you can't be in a wall
                unreachable();
            case TileType_UNKNOWN_FLOOR:
            case TileType_UNKNOWN_WALL:
            case TileType_UNKNOWN:
            case TileType_COUNT:
                unreachable();
        }
    }
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
        publish_event(Event::create(Event::DELETE_THING, delete_things[i]->id));

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

            {
                animate_map_tiles();
                Thing individual;
                for (auto iterator = actual_individuals(); iterator.next(&individual);)
                    see_aesthetics(individual);
            }

            age_things();

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
                if (can_move(individual) || can_act(individual)) {
                    poised_individuals.append(individual);
                    // log the passage of time in the message window.
                    // this actually only observers time in increments of your movement cost
                    List<Nullable<PerceivedEvent>> & events = individual->life()->knowledge.perceived_events;
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
                if (!individual->still_exists) {
                    // sorry, buddy. you were that close to making another move.
                    break;
                }
                Action action;
                if (!can_act(individual)) {
                    if (can_move(individual)) {
                        // still floating through the air at a fast speed waiting for action cost
                        action = Action::wait();
                    } else {
                        // someone interrupted your turn and made you lose it.
                        break;
                    }
                } else {
                    action = decision_makers[individual->life()->decision_maker](individual);
                }
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
