#include "item.hpp"

#include "thing.hpp"
#include "util.hpp"
#include "swarkland.hpp"
#include "event.hpp"

WandId actual_wand_identities[WandDescriptionId_COUNT];
PotionId actual_potion_identities[PotionDescriptionId_COUNT];

static Thing register_item(Thing item) {
    actual_things.put(item->id, item);
    return item;
}
static WandDescriptionId reverse_identify(WandId wand_id) {
    for (int i = 0; i < WandDescriptionId_COUNT; i++)
        if (actual_wand_identities[i] == wand_id)
            return (WandDescriptionId)i;
    panic("wand not found");
}
static PotionDescriptionId reverse_identify(PotionId potion_id) {
    for (int i = 0; i < PotionDescriptionId_COUNT; i++)
        if (actual_potion_identities[i] == potion_id)
            return (PotionDescriptionId)i;
    panic("potion not found");
}
Thing create_wand(WandId wand_id) {
    WandDescriptionId description_id = reverse_identify(wand_id);
    int charges = random_int(4, 8, "wand_charges");
    return register_item(create<ThingImpl>(description_id, charges));
}
Thing create_potion(PotionId potion_id) {
    PotionDescriptionId description_id = reverse_identify(potion_id);
    return register_item(create<ThingImpl>(description_id));
}

Thing create_random_item(ThingType thing_type) {
    switch (thing_type) {
        case ThingType_POTION:
            return create_potion((PotionId)random_int(PotionId_COUNT, nullptr));
        case ThingType_WAND:
            return create_wand((WandId)random_int(WandId_COUNT, nullptr));
        case ThingType_INDIVIDUAL:
            panic("not an item");

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
Thing create_random_item() {
    if (random_int(WandDescriptionId_COUNT + PotionDescriptionId_COUNT, nullptr) < WandDescriptionId_COUNT)
        return create_random_item(ThingType_WAND);
    else
        return create_random_item(ThingType_POTION);
}

// return how much extra beam length this happening requires.
// return -1 for stop the beam.
struct WandHandler {
    int (*hit_air)(Thing actor, Coord location, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper);
    int (*hit_individual)(Thing actor, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper);
    int (*hit_wall)(Thing actor, Coord location, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper);
};

static int pass_through_air_silently(Thing, Coord, bool, IdMap<WandDescriptionId> *) {
    return 0;
}
static int hit_individual_no_effect(Thing, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    publish_event(Event::wand_hit(WandId_UNKNOWN, is_explosion, target->id, target->location), perceived_current_zapper);
    return 0;
}
static int hit_wall_no_effect(Thing, Coord location, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    publish_event(Event::wand_hit(WandId_UNKNOWN, is_explosion, uint256::zero(), location), perceived_current_zapper);
    // and stop
    return -1;
}

static int confusion_hit_individual(Thing, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    bool confusable = individual_has_mind(target);
    publish_event(Event::wand_hit(confusable ? WandId_WAND_OF_CONFUSION : WandId_UNKNOWN, is_explosion, target->id, target->location), perceived_current_zapper);
    if (confusable)
        confuse_individual(target);
    return 2;
}
static int striking_hit_individual(Thing actor, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    publish_event(Event::wand_hit(WandId_WAND_OF_STRIKING, is_explosion, target->id, target->location), perceived_current_zapper);
    strike_individual(actor, target);
    return 2;
}
static int speed_hit_individual(Thing, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    publish_event(Event::wand_hit(WandId_WAND_OF_SPEED, is_explosion, target->id, target->location), perceived_current_zapper);
    speed_up_individual(target);
    return 2;
}
static int remedy_hit_individual(Thing, Thing target, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    int confusion_index = find_status(target->status_effects, StatusEffect::CONFUSION);
    int poison_index = find_status(target->status_effects, StatusEffect::POISON);
    int blindness_index = find_status(target->status_effects, StatusEffect::BLINDNESS);
    bool did_it_help = confusion_index != -1 || poison_index != -1 || blindness_index != -1;
    publish_event(Event::wand_hit(did_it_help ? WandId_WAND_OF_REMEDY : WandId_UNKNOWN, is_explosion, target->id, target->location), perceived_current_zapper);
    if (confusion_index != -1)
        target->status_effects[confusion_index].expiration_time = time_counter + 1;
    if (poison_index != -1)
        target->status_effects[poison_index].expiration_time = time_counter + 1;
    if (blindness_index != -1)
        target->status_effects[blindness_index].expiration_time = time_counter + 1;
    return 2;
}

static int digging_hit_wall(Thing, Coord location, bool is_explosion, IdMap<WandDescriptionId> * perceived_current_zapper) {
    if (actual_map_tiles[location].tile_type != TileType_WALL)
        return -1; // probably border walls
    publish_event(Event::wand_hit(WandId_WAND_OF_DIGGING, is_explosion, uint256::zero(), location), perceived_current_zapper);
    change_map(location, TileType_DIRT_FLOOR);
    return 0;
}
static int digging_pass_through_air(Thing, Coord, bool, IdMap<WandDescriptionId> *) {
    // the digging beam doesn't travel well through air
    return 2;
}

static WandHandler wand_handlers[WandId_COUNT];

void init_items() {
    assert((int)WandDescriptionId_COUNT == (int)WandId_COUNT);
    for (int i = 0; i < WandDescriptionId_COUNT; i++)
        actual_wand_identities[i] = (WandId)i;
    if (!test_mode)
        shuffle(actual_wand_identities, WandId_COUNT);

    assert((int)PotionDescriptionId_COUNT == (int)PotionId_COUNT);
    for (int i = 0; i < PotionDescriptionId_COUNT; i++)
        actual_potion_identities[i] = (PotionId)i;
    if (!test_mode)
        shuffle(actual_potion_identities, PotionId_COUNT);

    wand_handlers[WandId_WAND_OF_CONFUSION] = {pass_through_air_silently, confusion_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_DIGGING] = {digging_pass_through_air, hit_individual_no_effect, digging_hit_wall};
    wand_handlers[WandId_WAND_OF_STRIKING] = {pass_through_air_silently, striking_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_SPEED] = {pass_through_air_silently, speed_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_REMEDY] = {pass_through_air_silently, remedy_hit_individual, hit_wall_no_effect};
}

void delete_item(Thing item) {
    actual_things.remove(item->id);
    if (item->container_id != uint256::zero())
        fix_z_orders(item->container_id);
}

void zap_wand(Thing wand_wielder, uint256 item_id, Coord direction) {
    IdMap<WandDescriptionId> perceived_current_zapper;

    Thing wand = actual_things.get(item_id);
    wand->wand_info()->charges--;
    if (wand->wand_info()->charges <= -1) {
        publish_event(Event::wand_disintegrates(wand_wielder, wand), &perceived_current_zapper);

        actual_things.remove(item_id);
        fix_z_orders(wand_wielder->id);
        return;
    }
    if (wand->wand_info()->charges <= 0) {
        publish_event(Event::wand_zap_no_charges(wand_wielder, wand), &perceived_current_zapper);
        return;
    }

    publish_event(Event::zap_wand(wand_wielder, wand), &perceived_current_zapper);
    Coord cursor = wand_wielder->location;
    int beam_length = random_inclusive(beam_length_average - beam_length_error_margin, beam_length_average + beam_length_error_margin, "beam_length");
    WandHandler handler = wand_handlers[actual_wand_identities[wand->wand_info()->description_id]];
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;

        Thing target = find_individual_at(cursor);
        int length_penalty = 0;
        if (target != nullptr)
            length_penalty = handler.hit_individual(wand_wielder, target, false, &perceived_current_zapper);
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (!is_open_space(actual_map_tiles[cursor].tile_type))
            length_penalty = handler.hit_wall(wand_wielder, cursor, false, &perceived_current_zapper);
        else
            length_penalty = handler.hit_air(wand_wielder, cursor, false, &perceived_current_zapper);
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (direction == Coord{0, 0})
            break; // zapping yourself
    }
}

// is_breaking is used in the published event
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking) {
    uint256 target_id = target->id;
    Coord location = target->location;
    PotionId effect_id = actual_potion_identities[item->potion_info()->description_id];
    publish_event(Event::use_potion(item->id, effect_id, is_breaking, target_id, location));
    delete_item(item);
    switch (effect_id) {
        case PotionId_POTION_OF_HEALING: {
            int hp = target->life()->max_hitpoints() * 2 / 3;
            heal_hp(target, hp);
            break;
        }
        case PotionId_POTION_OF_POISON:
            poison_individual(actor, target);
            break;
        case PotionId_POTION_OF_ETHEREAL_VISION:
            find_or_put_status(target, StatusEffect::ETHEREAL_VISION)->expiration_time = time_counter + random_midpoint(2000, "potion_of_ethereal_vision_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_COGNISCOPY:
            find_or_put_status(target, StatusEffect::COGNISCOPY)->expiration_time = time_counter + random_midpoint(2000, "potion_of_cogniscopy_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_BLINDNESS:
            find_or_put_status(target, StatusEffect::BLINDNESS)->expiration_time = time_counter + random_midpoint(1000, "potion_of_blindness_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_INVISIBILITY:
            find_or_put_status(target, StatusEffect::INVISIBILITY)->expiration_time = time_counter + random_midpoint(2000, "potion_of_invisibility_expiration");
            break;

        case PotionId_COUNT:
        case PotionId_UNKNOWN:
            panic("not real ids");
    }
}

void explode_wand(Thing actor, Thing item, Coord explosion_center) {
    // boom
    WandId wand_id = actual_wand_identities[item->wand_info()->description_id];
    int apothem;
    if (wand_id == WandId_WAND_OF_DIGGING) {
        apothem = 2; // 5x5
    } else {
        apothem = 1; // 3x3
    }
    IdMap<WandDescriptionId> perceived_current_zapper;
    publish_event(Event::wand_explodes(item->id, explosion_center), &perceived_current_zapper);
    delete_item(item);

    List<Thing> affected_individuals;
    Thing individual;
    for (auto iterator = actual_individuals(); iterator.next(&individual);) {
        if (!individual->still_exists)
            continue;
        Coord abs_vector = abs(individual->location - explosion_center);
        if (max(abs_vector.x, abs_vector.y) <= apothem)
            affected_individuals.append(individual);
    }
    sort<Thing, compare_things_by_id>(affected_individuals.raw(), affected_individuals.length());

    List<Coord> affected_walls;
    Coord upper_left  = clamp(explosion_center - Coord{apothem, apothem}, {0, 0}, map_size - Coord{1, 1});
    Coord lower_right = clamp(explosion_center + Coord{apothem, apothem}, {0, 0}, map_size - Coord{1, 1});
    for (Coord wall_cursor = upper_left; wall_cursor.y <= lower_right.y; wall_cursor.y++)
        for (wall_cursor.x = upper_left.x; wall_cursor.x <= lower_right.x; wall_cursor.x++)
            if (actual_map_tiles[wall_cursor].tile_type == TileType_WALL)
                affected_walls.append(wall_cursor);

    WandHandler handler = wand_handlers[actual_wand_identities[item->wand_info()->description_id]];
    for (int i = 0; i < affected_individuals.length(); i++)
        handler.hit_individual(actor, affected_individuals[i], true, &perceived_current_zapper);
    for (int i = 0; i < affected_walls.length(); i++)
        handler.hit_wall(actor, affected_walls[i], true, &perceived_current_zapper);
}

void break_potion(Thing actor, Thing item, Coord location) {
    Thing target = find_individual_at(location);
    if (target != nullptr) {
        use_potion(actor, target, item, true);
    } else {
        publish_event(Event::use_potion(item->id, PotionId_UNKNOWN, true, uint256::zero(), location));
    }
    delete_item(item);
}
