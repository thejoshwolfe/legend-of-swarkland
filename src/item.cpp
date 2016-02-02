#include "item.hpp"

#include "thing.hpp"
#include "util.hpp"
#include "swarkland.hpp"
#include "event.hpp"

WandId actual_wand_identities[WandDescriptionId_COUNT];
PotionId actual_potion_identities[PotionDescriptionId_COUNT];
BookId actual_book_identities[BookDescriptionId_COUNT];

static Thing register_item(Thing item) {
    actual_things.put(item->id, item);
    return item;
}
static WandDescriptionId reverse_identify(WandId wand_id) {
    for (int i = 0; i < WandDescriptionId_COUNT; i++)
        if (actual_wand_identities[i] == wand_id)
            return (WandDescriptionId)i;
    unreachable();
}
static PotionDescriptionId reverse_identify(PotionId potion_id) {
    for (int i = 0; i < PotionDescriptionId_COUNT; i++)
        if (actual_potion_identities[i] == potion_id)
            return (PotionDescriptionId)i;
    unreachable();
}
static BookDescriptionId reverse_identify(BookId book_id) {
    for (int i = 0; i < BookDescriptionId_COUNT; i++)
        if (actual_book_identities[i] == book_id)
            return (BookDescriptionId)i;
    unreachable();
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
Thing create_book(BookId book_id) {
    BookDescriptionId description_id = reverse_identify(book_id);
    return register_item(create<ThingImpl>(description_id));
}

Thing create_random_item(ThingType thing_type) {
    switch (thing_type) {
        case ThingType_INDIVIDUAL:
            unreachable();
        case ThingType_WAND:
            return create_wand((WandId)random_int(WandId_COUNT, nullptr));
        case ThingType_POTION:
            return create_potion((PotionId)random_int(PotionId_COUNT, nullptr));
        case ThingType_BOOK:
            return create_book((BookId)random_int(BookId_COUNT, nullptr));

        case ThingType_COUNT:
            unreachable();
    }
    panic("thing type");
}
Thing create_random_item() {
    int thing_type_selector = random_int(WandDescriptionId_COUNT + PotionDescriptionId_COUNT + BookDescriptionId_COUNT, nullptr);
    if (thing_type_selector < WandDescriptionId_COUNT)
        return create_random_item(ThingType_WAND);
    else if (thing_type_selector < WandDescriptionId_COUNT + PotionDescriptionId_COUNT)
        return create_random_item(ThingType_POTION);
    else
        return create_random_item(ThingType_BOOK);
}

// return how much extra beam length this happening requires.
// return -1 for stop the beam.
struct MagicBeamHandler {
    int (*hit_air)(Thing actor, Coord location, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam);
    int (*hit_individual)(Thing actor, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam);
    int (*hit_wall)(Thing actor, Coord location, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam);
};

static int pass_through_air_silently(Thing, Coord, bool, IdMap<uint256> *) {
    return 0;
}
static int hit_individual_no_effect(Thing, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    publish_event(Event::magic_beam_hit(MagicBeamEffect_UNKNOWN, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    return 0;
}
static int hit_wall_no_effect(Thing, Coord location, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    publish_event(Event::magic_beam_hit(MagicBeamEffect_UNKNOWN, is_explosion, uint256::zero(), location), perceived_source_of_magic_beam);
    // and stop
    return -1;
}

static int confusion_hit_individual(Thing, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    bool confusable = individual_has_mind(target);
    publish_event(Event::magic_beam_hit(confusable ? MagicBeamEffect_CONFUSION : MagicBeamEffect_UNKNOWN, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    if (confusable)
        confuse_individual(target);
    return 2;
}
static int striking_hit_individual(Thing actor, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    publish_event(Event::magic_beam_hit(MagicBeamEffect_STRIKING, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    strike_individual(actor, target);
    return 2;
}
static int speed_hit_individual(Thing, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    publish_event(Event::magic_beam_hit(MagicBeamEffect_SPEED, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    speed_up_individual(target);
    return 2;
}
static int remedy_hit_individual(Thing, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    int confusion_index = find_status(target->status_effects, StatusEffect::CONFUSION);
    int poison_index = find_status(target->status_effects, StatusEffect::POISON);
    int blindness_index = find_status(target->status_effects, StatusEffect::BLINDNESS);
    bool did_it_help = confusion_index != -1 || poison_index != -1 || blindness_index != -1;
    publish_event(Event::magic_beam_hit(did_it_help ? MagicBeamEffect_REMEDY : MagicBeamEffect_UNKNOWN, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    if (confusion_index != -1)
        target->status_effects[confusion_index].expiration_time = time_counter + 1;
    if (poison_index != -1)
        target->status_effects[poison_index].expiration_time = time_counter + 1;
    if (blindness_index != -1)
        target->status_effects[blindness_index].expiration_time = time_counter + 1;
    return 2;
}
static int magic_bullet_hit_individual(Thing actor, Thing target, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    publish_event(Event::magic_beam_hit(MagicBeamEffect_MAGIC_BULLET, is_explosion, target->id, target->location), perceived_source_of_magic_beam);
    magic_bullet_hit_individual(actor, target);
    return -1;
}

static int digging_hit_wall(Thing, Coord location, bool is_explosion, IdMap<uint256> * perceived_source_of_magic_beam) {
    if (actual_map_tiles[location] == TileType_BORDER_WALL) {
        // no effect on border walls
        publish_event(Event::magic_beam_hit(MagicBeamEffect_UNKNOWN, is_explosion, uint256::zero(), location), perceived_source_of_magic_beam);
        return -1;
    }
    publish_event(Event::magic_beam_hit(MagicBeamEffect_DIGGING, is_explosion, uint256::zero(), location), perceived_source_of_magic_beam);
    change_map(location, TileType_DIRT_FLOOR);
    return 0;
}
static int digging_pass_through_air(Thing, Coord, bool, IdMap<uint256> *) {
    // the digging beam doesn't travel well through air
    return 2;
}

static MagicBeamHandler wand_handlers[WandId_COUNT];
static MagicBeamHandler book_handlers[BookId_COUNT];

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

    assert((int)BookDescriptionId_COUNT == (int)BookId_COUNT);
    for (int i = 0; i < BookDescriptionId_COUNT; i++)
        actual_book_identities[i] = (BookId)i;
    if (!test_mode)
        shuffle(actual_book_identities, BookId_COUNT);

    wand_handlers[WandId_WAND_OF_CONFUSION] = {pass_through_air_silently, confusion_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_DIGGING] = {digging_pass_through_air, hit_individual_no_effect, digging_hit_wall};
    wand_handlers[WandId_WAND_OF_STRIKING] = {pass_through_air_silently, striking_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_SPEED] = {pass_through_air_silently, speed_hit_individual, hit_wall_no_effect};
    wand_handlers[WandId_WAND_OF_REMEDY] = {pass_through_air_silently, remedy_hit_individual, hit_wall_no_effect};

    book_handlers[BookId_SPELLBOOK_OF_MAGIC_BULLET] = {pass_through_air_silently, magic_bullet_hit_individual, hit_wall_no_effect};
}

static void shoot_magic_beam(Thing wand_wielder, Coord direction, const MagicBeamHandler & handler, IdMap<uint256> * perceived_source_of_magic_beam) {
    Coord cursor = wand_wielder->location;
    int beam_length = random_inclusive(beam_length_average - beam_length_error_margin, beam_length_average + beam_length_error_margin, "beam_length");
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;

        Thing target = find_individual_at(cursor);
        int length_penalty = 0;
        if (target != nullptr)
            length_penalty = handler.hit_individual(wand_wielder, target, false, perceived_source_of_magic_beam);
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (!is_open_space(actual_map_tiles[cursor]))
            length_penalty = handler.hit_wall(wand_wielder, cursor, false, perceived_source_of_magic_beam);
        else
            length_penalty = handler.hit_air(wand_wielder, cursor, false, perceived_source_of_magic_beam);
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (direction == Coord{0, 0})
            break; // zapping yourself
    }
}

void zap_wand(Thing wand_wielder, uint256 item_id, Coord direction) {
    IdMap<uint256> perceived_source_of_magic_beam;

    Thing wand = actual_things.get(item_id);
    if (wand->wand_info()->charges <= -1) {
        publish_event(Event::wand_disintegrates(wand_wielder, wand), &perceived_source_of_magic_beam);

        wand->still_exists = false;
        return;
    }
    if (wand->wand_info()->charges <= 0) {
        publish_event(Event::wand_zap_no_charges(wand_wielder, wand), &perceived_source_of_magic_beam);
        wand->wand_info()->charges--;
        return;
    }
    wand->wand_info()->charges--;

    publish_event(Event::zap_wand(wand_wielder, wand), &perceived_source_of_magic_beam);
    MagicBeamHandler handler = wand_handlers[actual_wand_identities[wand->wand_info()->description_id]];
    shoot_magic_beam(wand_wielder, direction, handler, &perceived_source_of_magic_beam);
}

void read_book(Thing actor, uint256 item_id, Coord direction) {
    IdMap<uint256> perceived_source_of_magic_beam;

    Thing book = actual_things.get(item_id);

    publish_event(Event::read_book(actor, book), &perceived_source_of_magic_beam);
    // TODO: check for success
    MagicBeamHandler handler = book_handlers[actual_book_identities[book->book_info()->description_id]];
    shoot_magic_beam(actor, direction, handler, &perceived_source_of_magic_beam);
}

// is_breaking is used in the published event
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking) {
    uint256 target_id = target->id;
    PotionId effect_id = actual_potion_identities[item->potion_info()->description_id];
    // the potion might appear to do nothing
    PotionId apparent_effect = effect_id;
    switch (effect_id) {
        case PotionId_POTION_OF_HEALING:
            if (target->life()->hitpoints >= target->life()->max_hitpoints())
                apparent_effect = PotionId_UNKNOWN;
            break;
        case PotionId_POTION_OF_POISON:
            if (has_status(target, StatusEffect::POISON))
                apparent_effect = PotionId_UNKNOWN;
            break;
        case PotionId_POTION_OF_ETHEREAL_VISION:
            if (has_status(target, StatusEffect::ETHEREAL_VISION))
                apparent_effect = PotionId_UNKNOWN;
            break;
        case PotionId_POTION_OF_COGNISCOPY:
            if (has_status(target, StatusEffect::COGNISCOPY))
                apparent_effect = PotionId_UNKNOWN;
            break;
        case PotionId_POTION_OF_BLINDNESS:
            if (has_status(target, StatusEffect::BLINDNESS))
                apparent_effect = PotionId_UNKNOWN;
            break;
        case PotionId_POTION_OF_INVISIBILITY:
            if (has_status(target, StatusEffect::INVISIBILITY))
                apparent_effect = PotionId_UNKNOWN;
            break;

        case PotionId_COUNT:
        case PotionId_UNKNOWN:
            unreachable();
    }
    publish_event(Event::use_potion(item->id, apparent_effect, is_breaking, target_id));
    item->still_exists = false;
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
            unreachable();
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
    IdMap<uint256> perceived_source_of_magic_beam;
    publish_event(Event::wand_explodes(item->id, explosion_center), &perceived_source_of_magic_beam);
    item->still_exists = false;

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
            if (actual_map_tiles[wall_cursor] == TileType_WALL)
                affected_walls.append(wall_cursor);

    MagicBeamHandler handler = wand_handlers[actual_wand_identities[item->wand_info()->description_id]];
    for (int i = 0; i < affected_individuals.length(); i++)
        handler.hit_individual(actor, affected_individuals[i], true, &perceived_source_of_magic_beam);
    for (int i = 0; i < affected_walls.length(); i++)
        handler.hit_wall(actor, affected_walls[i], true, &perceived_source_of_magic_beam);
}

void break_potion(Thing actor, Thing item, Coord location) {
    Thing target = find_individual_at(location);
    if (target != nullptr) {
        use_potion(actor, target, item, true);
    } else {
        publish_event(Event::potion_breaks(item->id, location));
    }
    item->still_exists = false;
}
