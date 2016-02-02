#include "swarkland.hpp"

#include "path_finding.hpp"
#include "decision.hpp"
#include "display.hpp"
#include "event.hpp"
#include "item.hpp"
#include "tas.hpp"

Species specieses[SpeciesId_COUNT];
Mind specieses_mind[SpeciesId_COUNT];

Ability abilities[Ability::COUNT];
List<Ability::Id> species_abilities[SpeciesId_COUNT];

bool test_mode;
bool headless_mode;

IdMap<Thing> actual_things;

Thing you;
bool youre_still_alive = true;
int64_t time_counter = 0;
Thing player_actor;

bool cheatcode_full_visibility;

static int test_you_events_mark;
static List<PerceivedThing> test_expect_things_list;
static List<PerceivedThing> test_expect_carrying_list;

static void init_static_data() {
    VisionTypes norm = VisionTypes_NORMAL;
    VisionTypes ethe = VisionTypes_ETHEREAL;
    //                                     movement cost
    //                                     |   health
    //                                     |   |  base attack
    //                                     |   |  |  min level
    //                                     |   |  |  |   max level
    //                                     |   |  |  |   |  vision
    //                                     |   |  |  |   |  |     sucks up items
    //                                     |   |  |  |   |  |     |  auto throws items
    //                                     |   |  |  |   |  |     |  |  poison attack
    specieses[SpeciesId_HUMAN        ] = {12, 10, 3, 0, 10, norm, 0, 0, 0};
    specieses[SpeciesId_OGRE         ] = {24, 15, 2, 4, 10, norm, 0, 0, 0};
    specieses[SpeciesId_LICH         ] = {12, 12, 3, 7, 10, norm, 0, 0, 0};
    specieses[SpeciesId_PINK_BLOB    ] = {48,  4, 1, 0,  1, ethe, 1, 0, 0};
    specieses[SpeciesId_AIR_ELEMENTAL] = { 6,  6, 1, 3, 10, ethe, 1, 1, 0};
    specieses[SpeciesId_DOG          ] = {12,  4, 2, 1,  2, norm, 0, 0, 0};
    specieses[SpeciesId_ANT          ] = {12,  2, 1, 0,  1, norm, 0, 0, 0};
    specieses[SpeciesId_BEE          ] = {12,  2, 3, 1,  2, norm, 0, 0, 0};
    specieses[SpeciesId_BEETLE       ] = {24,  6, 1, 0,  1, norm, 0, 0, 0};
    specieses[SpeciesId_SCORPION     ] = {24,  5, 1, 2,  3, norm, 0, 0, 1};
    specieses[SpeciesId_SNAKE        ] = {18,  4, 2, 1,  2, norm, 0, 0, 0};
    specieses[SpeciesId_COBRA        ] = {18,  2, 1, 2,  3, norm, 0, 0, 0};
    for (int i = 0; i < SpeciesId_COUNT; i++)
        if (specieses[i].movement_cost == 0)
            panic("you missed a spot");

    specieses_mind[SpeciesId_HUMAN        ] = Mind_CIVILIZED;
    specieses_mind[SpeciesId_OGRE         ] = Mind_SAVAGE;
    specieses_mind[SpeciesId_LICH         ] = Mind_CIVILIZED;
    specieses_mind[SpeciesId_PINK_BLOB    ] = Mind_NONE;
    specieses_mind[SpeciesId_AIR_ELEMENTAL] = Mind_NONE;
    specieses_mind[SpeciesId_DOG          ] = Mind_BEAST;
    specieses_mind[SpeciesId_ANT          ] = Mind_BEAST;
    specieses_mind[SpeciesId_BEE          ] = Mind_BEAST;
    specieses_mind[SpeciesId_BEETLE       ] = Mind_BEAST;
    specieses_mind[SpeciesId_SCORPION     ] = Mind_BEAST;
    specieses_mind[SpeciesId_SNAKE        ] = Mind_BEAST;
    specieses_mind[SpeciesId_COBRA        ] = Mind_BEAST;

    abilities[Ability::SPIT_BLINDING_VENOM] = {Ability::SPIT_BLINDING_VENOM};

    species_abilities[SpeciesId_COBRA].append(Ability::SPIT_BLINDING_VENOM);
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

    if (individual == you)
        youre_still_alive = false;
    if (individual == cheatcode_spectator) {
        // our fun looking through the eyes of a dying man has ended. back to normal.
        cheatcode_spectator = nullptr;
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
    if (!test_mode && life->hitpoints < life->max_hitpoints())
        life->hp_regen_deadline = time_counter + random_midpoint(7 * 12, nullptr);
}
static void damage_individual(Thing target, int damage, Thing attacker, bool is_melee) {
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
    StatusEffect * poison = find_or_put_status(target, StatusEffect::POISON);
    poison->who_is_responsible = attacker->id;
    poison->expiration_time = time_counter + random_midpoint(600, "poison_expiriration");
    poison->poison_next_damage_time = time_counter + 12 * 3;
}
static void blind_individual(Thing attacker, Thing target) {
    StatusEffect * effect = find_or_put_status(target, StatusEffect::BLINDNESS);
    effect->who_is_responsible = attacker->id;
    effect->expiration_time = time_counter + random_midpoint(600, "blindness_expiriration");
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
    if (individual != nullptr && individual->life()->species()->sucks_up_items) {
        // suck it
        pickup_item(individual, item);
        publish_event(Event::individual_sucks_up_item(individual, item));
    }
}
static void throw_item(Thing actor, Thing item, Coord direction) {
    publish_event(Event::throw_item(actor, item->id));
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
    bool impacts_in_wall = item->thing_type == ThingType_WAND && actual_wand_identities[item->wand_info()->description_id] == WandId_WAND_OF_DIGGING;
    for (int i = 0; i < range; i++) {
        cursor += direction;
        if (!is_open_space(actual_map_tiles[cursor])) {
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
            publish_event(Event::item_hits_individual(item->id, target));
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

static void do_ability(Thing actor, Ability::Id ability_id, Coord direction) {
    int cooldown_time = random_midpoint(1000, "ability_cooldown");
    actor->ability_cooldowns.append(AbilityCooldown{ability_id, time_counter + cooldown_time});

    int range;
    switch (ability_id) {
        case Ability::SPIT_BLINDING_VENOM:
            publish_event(Event::spit_blinding_venom(actor->id));
            range = random_int(throw_distance_average - throw_distance_error_margin, throw_distance_average + throw_distance_error_margin, "spit_distance");
            break;
        case Ability::COUNT:
            unreachable();
    }
    Coord cursor = actor->location;
    for (int i = 0; i < range; i++) {
        cursor += direction;
        Thing target = find_individual_at(cursor);
        if (target != nullptr) {
            publish_event(Event::blinding_venom_hit_individual(target->id));
            blind_individual(actor, target);
            publish_event(Event::gain_status(target->id, StatusEffect::BLINDNESS));
            compute_vision(target);
            return;
        }
        if (!is_open_space(actual_map_tiles[cursor])) {
            // hit wall
            // TODO: publish event
            return;
        }
    }
    // falls on the floor
}

// SpeciesId_COUNT => random
// location = Coord::nowhere() => random
static Thing spawn_a_monster(SpeciesId species_id, DecisionMakerType decision_maker, Coord location) {
    if (species_id == SpeciesId_COUNT) {
        int difficulty = dungeon_level - random_triangle_distribution(dungeon_level);
        assert(0 <= difficulty && difficulty < dungeon_level);
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
        // don't spawn monsters near you. don't spawn you near the stairs.
        Coord away_from_location = you != nullptr ? you->location : stairs_down_location;
        location = random_spawn_location(away_from_location);
        if (location == Coord::nowhere()) {
            // it must be pretty crowded in here
            return nullptr;
        }
    }

    Thing individual = create<ThingImpl>(species_id, location, decision_maker);

    gain_experience(individual, level_to_experience(specieses[species_id].min_level + 1) - 1, false);

    actual_things.put(individual->id, individual);
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
    if (test_mode) {
        you = spawn_a_monster(SpeciesId_HUMAN, DecisionMakerType_PLAYER, Coord{1, 1});
        return;
    }

    if (you == nullptr) {
        you = spawn_a_monster(SpeciesId_HUMAN, DecisionMakerType_PLAYER, Coord::nowhere());
    } else {
        // you just landed from upstairs
        // make sure the up and down stairs are sufficiently far apart.
        you->location = random_spawn_location(stairs_down_location);
        compute_vision(you);
    }

    if (dungeon_level == final_dungeon_level) {
        // boss
        Thing boss = spawn_a_monster(SpeciesId_LICH, DecisionMakerType_AI, Coord::nowhere());
        // arm him!
        for (int i = 0; i < 5; i++) {
            pickup_item(boss, create_random_item(ThingType_WAND));
            pickup_item(boss, create_random_item(ThingType_POTION));
        }
        // teach him everything about magic.
        for (int i = 0; i < WandDescriptionId_COUNT; i++)
            boss->life()->knowledge.wand_identities[i] = actual_wand_identities[i];
        for (int i = 0; i < PotionDescriptionId_COUNT; i++)
            boss->life()->knowledge.potion_identities[i] = actual_potion_identities[i];
    } else {
        // spawn a "miniboss", which is just a specific monster on the stairs.
        SpeciesId miniboss_species_id = get_miniboss_species(dungeon_level);
        spawn_a_monster(miniboss_species_id, DecisionMakerType_AI, find_stairs_down_location());
    }

    // random monsters
    for (int i = 0; i < 4 + 3 * dungeon_level; i++)
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
    for (auto iterator = actual_things.value_iterator(); iterator.next(&thing);) {
        if (thing == you)
            continue; // you're cool
        if (thing->container_id == you->id)
            continue; // take it with you
        // leave this behind us
        thing->still_exists = false;
    }

    you->life()->knowledge.reset_map();
    generate_map();
    init_individuals();
}

static void maybe_spawn_monsters() {
    if (test_mode)
        return;
    if (random_int(2000, nullptr) == 0)
        spawn_random_individual();
}

void heal_hp(Thing individual, int hp) {
    Life * life = individual->life();
    if (life->hitpoints >= life->max_hitpoints()) {
        // you have enough hitpoints.
        return;
    }
    life->hitpoints = min(life->hitpoints + hp, life->max_hitpoints());
    reset_hp_regen_timeout(individual);
}
static void regen_hp(Thing individual) {
    Life * life = individual->life();
    int index;
    if ((index = find_status(individual->status_effects, StatusEffect::POISON)) != -1) {
        // poison damage instead
        StatusEffect * poison = &individual->status_effects[index];
        if (poison->poison_next_damage_time == time_counter) {
            // ouch
            Thing attacker = actual_things.get(poison->who_is_responsible, nullptr);
            if (attacker != nullptr && !attacker->still_exists)
                attacker = nullptr;
            damage_individual(individual, 1, attacker, false);
            poison->poison_next_damage_time = time_counter + random_midpoint(7 * 12, "poison_damage");
        }
    } else if (life->hp_regen_deadline == time_counter) {
        // hp regen
        int hp_heal = random_inclusive(1, max(1, life->max_hitpoints() / 5), nullptr);
        heal_hp(individual, hp_heal);
    }
}

// normal melee attack
static void attack(Thing attacker, Thing target) {
    publish_event(Event::attack_individual(attacker, target));
    int attack_power = attacker->life()->attack_power();
    int min_damage = (attack_power + 1) / 2;
    int damage = random_inclusive(min_damage, min_damage + attack_power / 2, "melee_damage");
    damage_individual(target, damage, attacker, true);
    reset_hp_regen_timeout(attacker);
    if (target->still_exists && attacker->life()->species()->poison_attack && random_int(4, "poison_attack") == 0) {
        publish_event(Event::gain_status(target->id, StatusEffect::POISON));
        poison_individual(attacker, target);
    }
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
    for (auto iterator = get_perceived_individuals(observer); iterator.next(&individual);)
        if (individual->location == location)
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
        if (!individual->still_exists)
            continue;
        if (individual->location.x == location.x && individual->location.y == location.y)
            return individual;
    }
    return nullptr;
}

void find_items_in_inventory(uint256 container_id, List<Thing> * output_sorted_list) {
    Thing item;
    for (auto iterator = actual_items(); iterator.next(&item);)
        if (item->container_id == container_id)
            output_sorted_list->append(item);
    sort<Thing, compare_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void find_items_in_inventory(Thing observer, uint256 container_id, List<PerceivedThing> * output_sorted_list) {
    PerceivedThing item;
    for (auto iterator = get_perceived_items(observer); iterator.next(&item);)
        if (item->container_id == container_id)
            output_sorted_list->append(item);
    sort<PerceivedThing, compare_perceived_things_by_z_order>(output_sorted_list->raw(), output_sorted_list->length());
}
void get_abilities(Thing individual, List<Ability::Id> * output_sorted_abilities) {
    output_sorted_abilities->append_all(species_abilities[individual->life()->species_id]);
}
bool is_ability_ready(Thing actor, Ability::Id ability_id) {
    const List<AbilityCooldown> & ability_cooldowns = actor->ability_cooldowns;
    for (int i = 0; ability_cooldowns.length(); i++)
        if (ability_cooldowns[i].ability_id == ability_id)
            return false;
    return true;
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
    publish_event(Event::move(mover, old_position));

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

static void cheatcode_kill(uint256 individual_id) {
    Thing individual = actual_things.get(individual_id);
    kill_individual(individual, nullptr, false);
}
static void cheatcode_polymorph(SpeciesId species_id) {
    publish_event(Event::polymorph(player_actor, species_id));
    Life * life = player_actor->life();
    int old_max_hitpoints = life->max_hitpoints();
    life->species_id = species_id;
    int new_max_hitpoints = life->max_hitpoints();
    life->hitpoints = life->hitpoints * new_max_hitpoints / old_max_hitpoints;
    compute_vision(player_actor);
}
Thing cheatcode_spectator;
void cheatcode_spectate() {
    Coord individual_at = get_mouse_tile(main_map_area);
    cheatcode_spectator = find_individual_at(individual_at);
}

bool can_move(Thing actor) {
    return actor->life()->last_movement_time + get_movement_cost(actor) <= time_counter;
}
static bool can_act(Thing actor) {
    return actor->life()->last_action_time + action_cost <= time_counter;
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
            if (species_abilities[actor->life()->species_id].index_of(data.ability_id) == -1)
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
            if (actor != player_actor)
                return false;
            return true;
        case Action::CHEATCODE_KILL:
            if (actor != player_actor)
                return false;
            if (actual_things.get(action.item()) == nullptr)
                return false;
            return true;
        case Action::CHEATCODE_WISH:
            if (actor != player_actor)
                return false;
            if (action.thing().thing_type == ThingType_INDIVIDUAL)
                return false;
            return true;
        case Action::CHEATCODE_GENERATE_MONSTER: {
            if (actor != player_actor)
                return false;
            const Action::GenerateMonster & data = action.generate_monster();
            Coord location = data.location;
            if (!(is_in_bounds(location) && is_open_space(actual_map_tiles[location])))
                return false;
            if (find_individual_at(location) != nullptr)
                return false;
            return true;
        }

        case Action::DIRECTIVE_MARK_EVENTS:
        case Action::DIRECTIVE_EXPECT_EVENT:
        case Action::DIRECTIVE_EXPECT_NO_EVENTS:
        case Action::DIRECTIVE_FIND_THINGS_AT:
        case Action::DIRECTIVE_EXPECT_THING:
        case Action::DIRECTIVE_EXPECT_NOTHING:
        case Action::DIRECTIVE_EXPECT_CARRYING:
        case Action::DIRECTIVE_EXPECT_CARRYING_NOTHING:
            if (!test_mode)
                return false;
            return true;

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

static String get_thing_description(const Action::Thing & thing) {
    String result = new_string();
    switch (thing.thing_type) {
        case ThingType_INDIVIDUAL:
            result->format("individual %s", get_species_name_str(thing.species_id));
            return result;
        case ThingType_WAND:
            result->append(get_wand_id_str(thing.wand_id));
            return result;
        case ThingType_POTION:
            result->append(get_potion_id_str(thing.potion_id));
            return result;
        case ThingType_BOOK:
            result->append(get_book_id_str(thing.book_id));
            return result;
        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}
static PerceivedThing expect_thing(const Action::Thing & expected_thing, List<PerceivedThing> * things_list) {
    if (things_list->length() == 0)
        test_expect_fail("expected %s, found nothing.", get_thing_description(expected_thing));
    for (int i = 0; i < things_list->length(); i++) {
        PerceivedThing thing = (*things_list)[i];
        if (thing->thing_type != expected_thing.thing_type)
            continue;
        switch (thing->thing_type) {
            case ThingType_INDIVIDUAL:
                if (thing->life()->species_id != expected_thing.species_id)
                    continue;
                break;
            case ThingType_WAND:
                if (thing->wand_info()->description_id == WandDescriptionId_UNSEEN) {
                    if (expected_thing.wand_id != WandId_UNKNOWN)
                        continue;
                } else {
                    if (actual_wand_identities[thing->wand_info()->description_id] != expected_thing.wand_id)
                        continue;
                }
                break;
            case ThingType_POTION:
                if (thing->potion_info()->description_id == PotionDescriptionId_UNSEEN) {
                    if (expected_thing.potion_id != PotionId_UNKNOWN)
                        continue;
                } else {
                    if (actual_potion_identities[thing->potion_info()->description_id] != expected_thing.potion_id)
                        continue;
                }
                break;
            case ThingType_BOOK:
                if (thing->book_info()->description_id == BookDescriptionId_UNSEEN) {
                    if (expected_thing.book_id != BookId_UNKNOWN)
                        continue;
                } else {
                    if (actual_book_identities[thing->book_info()->description_id] != expected_thing.book_id)
                        continue;
                }
                break;
            case ThingType_COUNT:
                unreachable();
        }
        // found it
        things_list->swap_remove(i);
        return thing;
    }
    // didn't find it
    test_expect_fail("expected %s, found other things.", get_thing_description(expected_thing));
}
static void expect_nothing(const List<PerceivedThing> & things) {
    if (things.length() == 0)
        return;
    String thing_description = new_string();
    Span span = get_thing_description(you, things[0]->id);
    span->to_string(thing_description);
    test_expect_fail("expected nothing. found at least: %s.", thing_description);
}
static void skip_blank_events(const List<RememberedEvent> & events) {
    while (test_you_events_mark < events.length() && events[test_you_events_mark] == nullptr)
        test_you_events_mark++;
}
// return true if time should pass, false if we used an instantaneous cheatcode.
static bool take_action(Thing actor, const Action & action) {
    assert(validate_action(actor, action));
    // we know you can attempt the action, but it won't necessarily turn out the way you expected it.

    switch (action.id) {
        case Action::WAIT:
            break;
        case Action::MOVE: {
            // normally, we'd be sure that this was valid, but if you use cheatcodes,
            // monsters can try to walk into you while you're invisible.
            Coord new_position = actor->location + confuse_direction(actor, action.coord());
            if (!is_open_space(actual_map_tiles[new_position])) {
                // this can only happen if your direction was changed.
                // (attempting to move into a wall deliberately is an invalid move).
                publish_event(Event::bump_into_location(actor, new_position, false));
                break;
            }
            Thing target = find_individual_at(new_position);
            if (target != nullptr) {
                // this is not attacking
                publish_event(Event::bump_into_individual(actor, target));
                break;
            }
            // clear to move
            do_move(actor, new_position);
            break;
        }
        case Action::ATTACK: {
            Coord new_position = actor->location + confuse_direction(actor, action.coord());
            Thing target = find_individual_at(new_position);
            if (target != nullptr) {
                attack(actor, target);
                break;
            } else {
                bool is_air = is_open_space(actual_map_tiles[new_position]);
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
            pickup_item(actor, actual_things.get(action.item()));
            publish_event(Event::individual_picks_up_item(actor, action.item()));
            break;
        case Action::DROP:
            drop_item_to_the_floor(actual_things.get(action.item()), actor->location);
            break;
        case Action::QUAFF: {
            Thing item = actual_things.get(action.item());
            use_potion(actor, actor, item, false);
            break;
        }
        case Action::THROW:
            throw_item(actor, actual_things.get(action.coord_and_item().item), confuse_direction(actor, action.coord_and_item().coord));
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
            cheatcode_polymorph(action.species());
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
            for (int i = 0; i < WandDescriptionId_COUNT; i++)
                player_actor->life()->knowledge.wand_identities[i] = actual_wand_identities[i];
            for (int i = 0; i < PotionDescriptionId_COUNT; i++)
                player_actor->life()->knowledge.potion_identities[i] = actual_potion_identities[i];
            return false;
        case Action::CHEATCODE_GO_DOWN:
            if (dungeon_level < final_dungeon_level)
                go_down();
            break;
        case Action::CHEATCODE_GAIN_LEVEL:
            level_up(actor, true);
            return false;

        case Action::DIRECTIVE_MARK_EVENTS:
            test_you_events_mark = you->life()->knowledge.remembered_events.length();
            return false;
        case Action::DIRECTIVE_EXPECT_EVENT: {
            const List<RememberedEvent> & events = you->life()->knowledge.remembered_events;
            skip_blank_events(events);
            if (test_you_events_mark >= events.length())
                test_expect_fail("no new events.");
            const RememberedEvent & event = events[test_you_events_mark];
            String actual_text = new_string();
            event->span->to_string(actual_text);
            String expected_text = action.string();
            if (*actual_text != *expected_text)
                test_expect_fail("expected event text \"%s\". got \"%s\".", expected_text, actual_text);
            test_you_events_mark++;
            return false;
        }
        case Action::DIRECTIVE_EXPECT_NO_EVENTS: {
            const List<RememberedEvent> & events = you->life()->knowledge.remembered_events;
            skip_blank_events(events);
            if (test_you_events_mark < events.length()) {
                String event_text = new_string();
                events[test_you_events_mark]->span->to_string(event_text);
                test_expect_fail("expected no events. got \"%s\".", event_text);
            }
            return false;
        }
        case Action::DIRECTIVE_FIND_THINGS_AT:
            test_expect_things_list.clear();
            find_perceived_things_at(you, action.coord(), &test_expect_things_list);
            return false;
        case Action::DIRECTIVE_EXPECT_THING: {
            PerceivedThing thing = expect_thing(action.thing(), &test_expect_things_list);
            test_expect_carrying_list.clear();
            find_items_in_inventory(you, thing->id, &test_expect_carrying_list);
            return false;
        }
        case Action::DIRECTIVE_EXPECT_NOTHING:
            expect_nothing(test_expect_things_list);
            return false;
        case Action::DIRECTIVE_EXPECT_CARRYING:
            expect_thing(action.thing(), &test_expect_carrying_list);
            return false;
        case Action::DIRECTIVE_EXPECT_CARRYING_NOTHING:
            expect_nothing(test_expect_carrying_list);
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
            life->last_movement_time = time_counter + lowest_cost - movement_cost;
            life->last_action_time = time_counter + lowest_cost - action_cost;
        } else if (can_move(actor)) {
            int lowest_cost = min(movement_cost, action_cost - (int)(time_counter - life->last_action_time));
            life->last_movement_time += lowest_cost;
        } else {
            // can act
            int lowest_cost = min(action_cost, movement_cost - (int)(time_counter - life->last_movement_time));
            life->last_action_time += lowest_cost;
        }
    } else if (action.id == Action::MOVE) {
        life->last_movement_time = time_counter;
        if (movement_cost == action_cost) {
            life->last_action_time = time_counter;
        } else if (movement_cost > action_cost) {
            // slow individual.
            // wait action_cost until you can act, and even longer before you can move again.
            life->last_action_time = time_counter;
        } else {
            // fast individual
            if (life->last_action_time < life->last_movement_time + movement_cost - action_cost)
                life->last_action_time = life->last_movement_time + movement_cost - action_cost;
        }
    } else {
        if (!can_act(actor))
            panic("can't act");
        // action
        life->last_action_time = time_counter;
        // you have to wait for the action before you can move again.
        if (life->last_movement_time < life->last_action_time + action_cost - movement_cost)
            life->last_movement_time = life->last_action_time + action_cost - movement_cost;
    }

    return true;
}

// advance time for an individual
static void age_individual(Thing individual) {
    if (!test_mode)
        regen_hp(individual);

    if (individual->life()->species()->auto_throws_items) {
        // there's a chance per item they're carrying
        List<Thing> inventory;
        find_items_in_inventory(individual->id, &inventory);
        for (int i = 0; i < inventory.length(); i++) {
            if (!test_mode && random_int(100, nullptr) == 0) {
                // throw item in random direction
                throw_item(individual, inventory[i], directions[random_int(8, nullptr)]);
            }
        }
    }

    for (int i = 0; i < individual->status_effects.length(); i++) {
        StatusEffect status_effect = individual->status_effects[i];
        assert(status_effect.expiration_time >= time_counter);
        if (status_effect.expiration_time == time_counter) {
            individual->status_effects.swap_remove(i);
            i--;
            switch (status_effect.type) {
                case StatusEffect::CONFUSION:
                    publish_event(Event::lose_status(individual->id, StatusEffect::CONFUSION));
                    break;
                case StatusEffect::SPEED:
                    publish_event(Event::lose_status(individual->id, StatusEffect::SPEED));
                    break;
                case StatusEffect::ETHEREAL_VISION:
                    publish_event(Event::lose_status(individual->id, StatusEffect::ETHEREAL_VISION));
                    compute_vision(individual);
                    break;
                case StatusEffect::COGNISCOPY:
                    publish_event(Event::lose_status(individual->id, StatusEffect::COGNISCOPY));
                    compute_vision(individual);
                    break;
                case StatusEffect::BLINDNESS:
                    publish_event(Event::lose_status(individual->id, StatusEffect::BLINDNESS));
                    compute_vision(individual);
                    break;
                case StatusEffect::POISON:
                    publish_event(Event::lose_status(individual->id, StatusEffect::POISON));
                    reset_hp_regen_timeout(individual);
                    break;
                case StatusEffect::INVISIBILITY:
                    publish_event(Event::lose_status(individual->id, StatusEffect::INVISIBILITY));
                    break;
            }
        }
    }

    for (int i = 0; i < individual->ability_cooldowns.length(); i++) {
        assert(individual->ability_cooldowns[i].expiration_time >= time_counter);
        if (individual->ability_cooldowns[i].expiration_time == time_counter) {
            individual->ability_cooldowns.swap_remove(i);
            i--;
        }
    }

    List<RememberedEvent> & remembered_events = individual->life()->knowledge.remembered_events;
    if (remembered_events.length() >= 1000) {
        remembered_events.remove_range(0, 500);
        individual->life()->knowledge.event_forget_counter++;
        if (test_you_events_mark != 0)
            panic("events overflowed during mark/expect");
    }

    // clean up stale placeholders
    List<uint256> delete_ids;
    PerceivedThing thing;
    for (auto iterator = individual->life()->knowledge.perceived_things.value_iterator(); iterator.next(&thing);) {
        if (thing->thing_type == ThingType_INDIVIDUAL && thing->is_placeholder) {
            if (time_counter - thing->last_seen_time >= 12 * 20) {
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
        for (auto iterator = actual_things.value_iterator(); iterator.next(&thing);)
            if (!thing->still_exists)
                delete_things.append(thing);
    }
    List<uint256> fix_z_order_container_ids;
    for (int i = 0; i < delete_things.length(); i++) {
        Thing thing = delete_things[i];
        actual_things.remove(thing->id);
        if (thing->container_id != uint256::zero())
            if (fix_z_order_container_ids.index_of(thing->container_id) == -1)
                fix_z_order_container_ids.append(thing->container_id);
    }
    for (int i = 0; i < fix_z_order_container_ids.length(); i++)
        fix_z_orders(fix_z_order_container_ids[i]);
    // tell everyone to forget about this stuff.
    // TODO: delay this a random amount of time to hide information
    for (int i = 0; i < delete_things.length(); i++)
        publish_event(Event::delete_thing(delete_things[i]->id));
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
            }
        }
        poised_individuals.clear();
        poised_individuals_index = 0;

        delete_dead_things();
    }
    tas_delete_save();
}

// you need to emit events yourself
void confuse_individual(Thing target) {
    find_or_put_status(target, StatusEffect::CONFUSION)->expiration_time = time_counter + random_int(100, 200, "confusion_duration");
}
// you need to emit events yourself
void speed_up_individual(Thing target) {
    find_or_put_status(target, StatusEffect::SPEED)->expiration_time = time_counter + random_int(100, 200, "speed_duration");
}

void strike_individual(Thing attacker, Thing target) {
    int damage = random_int(4, 8, "striking_damage");
    damage_individual(target, damage, attacker, false);
}
void magic_bullet_hit_individual(Thing attacker, Thing target) {
    int damage = random_inclusive(1, 2, "magic_bullet_damage");
    damage_individual(target, damage, attacker, false);
}
void change_map(Coord location, TileType new_tile_type) {
    actual_map_tiles[location] = new_tile_type;
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
