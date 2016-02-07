#include "item.hpp"

#include "thing.hpp"
#include "util.hpp"
#include "swarkland.hpp"
#include "event.hpp"

WandDescriptionId actual_wand_descriptions[WandId_COUNT];
PotionDescriptionId actual_potion_descriptions[PotionId_COUNT];
BookDescriptionId actual_book_descriptions[BookId_COUNT];

static Thing register_item(Thing item) {
    actual_things.put(item->id, item);
    return item;
}
Thing create_wand(WandId wand_id) {
    int charges = random_int(4, 8, "wand_charges");
    return register_item(create<ThingImpl>(wand_id, charges));
}
Thing create_potion(PotionId potion_id) {
    return register_item(create<ThingImpl>(potion_id));
}
Thing create_book(BookId book_id) {
    return register_item(create<ThingImpl>(book_id));
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

static int hit_individual_no_effect(Thing target) {
    publish_event(Event::magic_beam_hit(target->id));
    return 0;
}
static int hit_wall_no_effect(Coord location) {
    publish_event(Event::magic_beam_hit_wall(location));
    // and stop
    return -1;
}

static int confusion_hit_individual(Thing target) {
    publish_event(Event::magic_beam_hit(target->id));
    publish_event(Event::gain_status(target->id, StatusEffect::CONFUSION));
    find_or_put_status(target, StatusEffect::CONFUSION)->expiration_time = time_counter + random_int(100, 200, "confusion_duration");
    return 2;
}
static int magic_missile_hit_individual(Thing actor, Thing target) {
    publish_event(Event::magic_missile_hit(target->id));
    int damage = random_int(4, 8, "magic_missile_damage");
    damage_individual(target, damage, actor, false);
    return 2;
}
static int speed_hit_individual(Thing target) {
    publish_event(Event::magic_beam_hit(target->id));
    publish_event(Event::gain_status(target->id, StatusEffect::SPEED));
    find_or_put_status(target, StatusEffect::SPEED)->expiration_time = time_counter + random_int(100, 200, "speed_duration");
    return 2;
}
static void remedy_status_effect(Thing individual, StatusEffect::Id status) {
    int index = find_status(individual->status_effects, status);
    if (index != -1) {
        individual->status_effects[index].expiration_time = time_counter;
        check_for_status_expired(individual, index);
    }
}
static int remedy_hit_individual(Thing target) {
    publish_event(Event::magic_beam_hit(target->id));

    remedy_status_effect(target, StatusEffect::CONFUSION);
    remedy_status_effect(target, StatusEffect::POISON);
    remedy_status_effect(target, StatusEffect::BLINDNESS);

    return 2;
}
static int magic_bullet_hit_individual(Thing actor, Thing target) {
    publish_event(Event::magic_bullet_hit(target->id));
    int damage = random_inclusive(1, 2, "magic_bullet_damage");
    damage_individual(target, damage, actor, false);
    return -1;
}

static int digging_hit_wall(Coord location) {
    if (actual_map_tiles[location] == TileType_BORDER_WALL) {
        // no effect on border walls
        publish_event(Event::magic_beam_hit_wall(location));
        return -1;
    }
    publish_event(Event::beam_of_digging_digs_wall(location));
    change_map(location, TileType_DIRT_FLOOR);
    return 0;
}

static int force_hit_individual(Thing target, Coord direction, int beam_length_remaining) {
    Coord cursor = target->location;
    List<Thing> choo_choo_train;
    for (int push_length = 0; push_length < beam_length_remaining; push_length++) {
        target = find_individual_at(cursor);
        if (target != nullptr) {
            // join the train!
            publish_event(Event::magic_beam_push_individual(target->id));
            choo_choo_train.append(target);
        } else {
            // move the train into this space
            for (int i = choo_choo_train.length() - 1; i >= 0; i--)
                attempt_move(choo_choo_train[i], choo_choo_train[i]->location + direction);
            if (!is_open_space(actual_map_tiles[cursor])) {
                // end of the line.
                // we just published a bunch of bump-into events.
                break;
            }
        }
        cursor += direction;
    }
    return -1;
}

void init_items() {
    assert((int)WandDescriptionId_COUNT == (int)WandId_COUNT);
    for (int i = 0; i < WandId_COUNT; i++)
        actual_wand_descriptions[i] = (WandDescriptionId)i;
    if (!test_mode)
        shuffle(actual_wand_descriptions, WandId_COUNT);

    assert((int)PotionDescriptionId_COUNT == (int)PotionId_COUNT);
    for (int i = 0; i < PotionId_COUNT; i++)
        actual_potion_descriptions[i] = (PotionDescriptionId)i;
    if (!test_mode)
        shuffle(actual_potion_descriptions, PotionId_COUNT);

    assert((int)BookDescriptionId_COUNT == (int)BookId_COUNT);
    for (int i = 0; i < BookId_COUNT; i++)
        actual_book_descriptions[i] = (BookDescriptionId)i;
    if (!test_mode)
        shuffle(actual_book_descriptions, BookId_COUNT);
}

enum ProjectileId {
    ProjectileId_BEAM_OF_CONFUSION,
    ProjectileId_BEAM_OF_DIGGING,
    ProjectileId_MAGIC_MISSILE,
    ProjectileId_BEAM_OF_SPEED,
    ProjectileId_BEAM_OF_REMEDY,
    ProjectileId_MAGIC_BULLET,
    ProjectileId_BEAM_OF_FORCE,
};

// functions should return how much extra beam length this happening requires.
// return -1 for stop the beam.
static void shoot_magic_beam(Thing actor, Coord direction, ProjectileId projectile_id) {
    Coord cursor = actor->location;
    int beam_length = random_inclusive(beam_length_average - beam_length_error_margin, beam_length_average + beam_length_error_margin, "beam_length");
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;

        Thing target = find_individual_at(cursor);
        int length_penalty = 0;
        if (target != nullptr) {
            // hit individual
            switch (projectile_id) {
                case ProjectileId_BEAM_OF_CONFUSION:
                    length_penalty = confusion_hit_individual(target);
                    break;
                case ProjectileId_BEAM_OF_DIGGING:
                    length_penalty = hit_individual_no_effect(target);
                    break;
                case ProjectileId_MAGIC_MISSILE:
                    length_penalty = magic_missile_hit_individual(actor, target);
                    break;
                case ProjectileId_BEAM_OF_SPEED:
                    length_penalty = speed_hit_individual(target);
                    break;
                case ProjectileId_BEAM_OF_REMEDY:
                    length_penalty = remedy_hit_individual(target);
                    break;
                case ProjectileId_MAGIC_BULLET:
                    length_penalty = magic_bullet_hit_individual(actor, target);
                    break;
                case ProjectileId_BEAM_OF_FORCE:
                    length_penalty = force_hit_individual(target, direction, beam_length - i);
                    break;
            }
        }
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (!is_open_space(actual_map_tiles[cursor])) {
            // hit wall
            switch (projectile_id) {
                case ProjectileId_BEAM_OF_DIGGING:
                    length_penalty = digging_hit_wall(cursor);
                    break;
                case ProjectileId_BEAM_OF_CONFUSION:
                case ProjectileId_MAGIC_MISSILE:
                case ProjectileId_BEAM_OF_SPEED:
                case ProjectileId_BEAM_OF_REMEDY:
                case ProjectileId_MAGIC_BULLET:
                case ProjectileId_BEAM_OF_FORCE: // TODO: force hit wall
                    length_penalty = hit_wall_no_effect(cursor);
                    break;
            }
        } else {
            // hit air
            switch (projectile_id) {
                case ProjectileId_BEAM_OF_DIGGING:
                    // the digging beam doesn't travel well through air
                    length_penalty = 2;
                    break;
                case ProjectileId_BEAM_OF_CONFUSION:
                case ProjectileId_MAGIC_MISSILE:
                case ProjectileId_BEAM_OF_SPEED:
                case ProjectileId_BEAM_OF_REMEDY:
                case ProjectileId_MAGIC_BULLET:
                case ProjectileId_BEAM_OF_FORCE:
                    length_penalty = 0;
                    break;
            }
        }
        if (length_penalty == -1)
            break;
        beam_length -= length_penalty;

        if (direction == Coord{0, 0})
            break; // zapping yourself
    }
}

static const int small_mapping_radius = 20;
static const int large_mapping_radius = 40;
static void do_mapping(Thing actor, Coord direction) {
    publish_event(Event::activated_mapping(actor->id));

    Coord center = actor->location;
    MapMatrix<TileType> & tiles = actor->life()->knowledge.tiles;
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            if (direction == Coord{0, 0}) {
                // small circle around you
                if (euclidean_distance_squared(center, cursor) > small_mapping_radius * small_mapping_radius)
                    continue;
            } else if (direction.x * direction.y == 0) {
                // large quarter circle projected from you with diagonal bounds
                Coord vector = cursor - center + direction;
                if (!(sign(vector.x + vector.y) == direction.x + direction.y &&
                      sign(vector.x - vector.y) == direction.x - direction.y))
                    continue;
                if (euclidean_distance_squared(center, cursor) > large_mapping_radius * large_mapping_radius)
                    continue;
            } else {
                // large quarter circle projected from you with orthogonal bounds
                if (sign(cursor - center + direction) != direction)
                    continue;
                if (euclidean_distance_squared(center, cursor) > large_mapping_radius * large_mapping_radius)
                    continue;
            }
            record_shape_of_terrain(&tiles, cursor);
        }
    }
}

void zap_wand(Thing actor, uint256 item_id, Coord direction) {
    Thing wand = actual_things.get(item_id);
    if (wand->wand_info()->charges <= -1) {
        publish_event(Event::wand_disintegrates(actor->id, item_id));

        wand->still_exists = false;
        return;
    }
    if (wand->wand_info()->charges <= 0) {
        publish_event(Event::wand_zap_no_charges(actor->id, item_id));
        wand->wand_info()->charges--;
        return;
    }
    wand->wand_info()->charges--;

    publish_event(Event::zap_wand(actor->id, item_id));
    switch (wand->wand_info()->wand_id) {
        case WandId_WAND_OF_CONFUSION:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_CONFUSION);
            break;
        case WandId_WAND_OF_DIGGING:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_DIGGING);
            break;
        case WandId_WAND_OF_MAGIC_MISSILE:
            shoot_magic_beam(actor, direction, ProjectileId_MAGIC_MISSILE);
            break;
        case WandId_WAND_OF_SPEED:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_SPEED);
            break;
        case WandId_WAND_OF_REMEDY:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_REMEDY);
            break;

        case WandId_COUNT:
        case WandId_UNKNOWN:
            unreachable();
    }

    observer_to_active_identifiable_item.clear();
}

int get_mana_cost(BookId book_id) {
    switch (book_id) {
        case BookId_SPELLBOOK_OF_MAGIC_BULLET:
            return 2;
        case BookId_SPELLBOOK_OF_SPEED:
            return 6;
        case BookId_SPELLBOOK_OF_MAPPING:
            return 10;
        case BookId_SPELLBOOK_OF_FORCE:
            return 4;

        case BookId_COUNT:
        case BookId_UNKNOWN:
            unreachable();
    }
    unreachable();
}

void read_book(Thing actor, uint256 item_id, Coord direction) {
    publish_event(Event::read_book(actor->id, item_id));

    Thing book = actual_things.get(item_id);
    BookId book_id = book->book_info()->book_id;
    // TODO: check for success
    int mana_cost = get_mana_cost(book_id);
    use_mana(actor, mana_cost);
    switch (book_id) {
        case BookId_SPELLBOOK_OF_MAGIC_BULLET:
            shoot_magic_beam(actor, direction, ProjectileId_MAGIC_BULLET);
            break;
        case BookId_SPELLBOOK_OF_SPEED:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_SPEED);
            break;
        case BookId_SPELLBOOK_OF_MAPPING:
            do_mapping(actor, direction);
            break;
        case BookId_SPELLBOOK_OF_FORCE:
            shoot_magic_beam(actor, direction, ProjectileId_BEAM_OF_FORCE);
            break;

        case BookId_COUNT:
        case BookId_UNKNOWN:
            unreachable();
    }
    observer_to_active_identifiable_item.clear();
}

// is_breaking is used in the published event
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking) {
    uint256 target_id = target->id;
    PotionId potion_id = item->potion_info()->potion_id;
    // the potion might appear to do nothing
    if (is_breaking)
        publish_event(Event::potion_hits_individual(target_id, item->id));
    else
        publish_event(Event::quaff_potion(target_id, item->id));
    item->still_exists = false;
    switch (potion_id) {
        case PotionId_POTION_OF_HEALING: {
            publish_event(Event::individual_is_healed(target_id));
            int hp = target->max_hitpoints() * 2 / 3;
            heal_hp(target, hp);
            break;
        }
        case PotionId_POTION_OF_POISON:
            poison_individual(actor, target);
            break;
        case PotionId_POTION_OF_ETHEREAL_VISION:
            publish_event(Event::gain_status(target_id, StatusEffect::ETHEREAL_VISION));
            find_or_put_status(target, StatusEffect::ETHEREAL_VISION)->expiration_time = time_counter + random_midpoint(2000, "potion_of_ethereal_vision_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_COGNISCOPY:
            publish_event(Event::gain_status(target_id, StatusEffect::COGNISCOPY));
            find_or_put_status(target, StatusEffect::COGNISCOPY)->expiration_time = time_counter + random_midpoint(2000, "potion_of_cogniscopy_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_BLINDNESS:
            publish_event(Event::gain_status(target_id, StatusEffect::BLINDNESS));
            find_or_put_status(target, StatusEffect::BLINDNESS)->expiration_time = time_counter + random_midpoint(1000, "potion_of_blindness_expiration");
            compute_vision(target);
            break;
        case PotionId_POTION_OF_INVISIBILITY:
            publish_event(Event::gain_status(target_id, StatusEffect::INVISIBILITY));
            find_or_put_status(target, StatusEffect::INVISIBILITY)->expiration_time = time_counter + random_midpoint(2000, "potion_of_invisibility_expiration");
            break;

        case PotionId_COUNT:
        case PotionId_UNKNOWN:
            unreachable();
    }
    observer_to_active_identifiable_item.clear();
}

void explode_wand(Thing actor, Thing wand, Coord explosion_center) {
    // boom
    WandId wand_id = wand->wand_info()->wand_id;
    int apothem;
    if (wand_id == WandId_WAND_OF_DIGGING) {
        apothem = 2; // 5x5
    } else {
        apothem = 1; // 3x3
    }
    publish_event(Event::wand_explodes(wand->id, explosion_center));
    wand->still_exists = false;

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

    switch (wand->wand_info()->wand_id) {
        case WandId_WAND_OF_CONFUSION:
            for (int i = 0; i < affected_individuals.length(); i++)
                confusion_hit_individual(affected_individuals[i]);
            break;
        case WandId_WAND_OF_DIGGING:
            for (int i = 0; i < affected_walls.length(); i++)
                digging_hit_wall(affected_walls[i]);
            break;
        case WandId_WAND_OF_MAGIC_MISSILE:
            for (int i = 0; i < affected_individuals.length(); i++)
                magic_missile_hit_individual(actor, affected_individuals[i]);
            break;
        case WandId_WAND_OF_SPEED:
            for (int i = 0; i < affected_individuals.length(); i++)
                speed_hit_individual(affected_individuals[i]);
            break;
        case WandId_WAND_OF_REMEDY:
            for (int i = 0; i < affected_individuals.length(); i++)
                remedy_hit_individual(affected_individuals[i]);
            break;

        case WandId_COUNT:
        case WandId_UNKNOWN:
            unreachable();
    }

    observer_to_active_identifiable_item.clear();
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
