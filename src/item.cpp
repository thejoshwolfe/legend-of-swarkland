#include "item.hpp"

#include "individual.hpp"
#include "util.hpp"
#include "swarkland.hpp"
#include "event.hpp"

WandId actual_wand_identities[WandId_COUNT];

void init_items() {
    for (int i = 0; i < WandId_COUNT; i++)
        actual_wand_identities[i] = (WandId)i;
    shuffle(actual_wand_identities, WandId_COUNT);
}

Thing random_item() {
    WandDescriptionId description_id = (WandDescriptionId)random_int(WandDescriptionId_COUNT);
    int charges = random_int(4, 8);
    Thing item = create<ThingImpl>(description_id, charges);
    actual_things.put(item->id, item);
    return item;
}

static void confuse_individual_from_wand(Thing target, IdMap<WandDescriptionId> * perceived_current_zapper) {
    if (target->life()->species()->has_mind) {
        publish_event(Event::beam_of_confusion_hit_individual(target), perceived_current_zapper);
        confuse_individual(target);
    } else {
        publish_event(Event::beam_hit_individual_no_effect(target), perceived_current_zapper);
    }
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
    int beam_length = random_inclusive(beam_length_average - beam_length_error_margin, beam_length_average + beam_length_error_margin);
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;
        switch (actual_wand_identities[wand->wand_info()->description_id]) {
            case WandId_WAND_OF_DIGGING: {
                if (actual_map_tiles[cursor].tile_type == TileType_WALL) {
                    publish_event(Event::beam_of_digging_hit_wall(cursor), &perceived_current_zapper);
                    change_map(cursor, TileType_FLOOR);
                } else {
                    // the digging beam doesn't travel well through air
                    beam_length -= 3;
                    Thing target = find_individual_at(cursor);
                    if (target != NULL)
                        publish_event(Event::beam_hit_individual_no_effect(target));
                }
                break;
            }
            case WandId_WAND_OF_STRIKING: {
                Thing target = find_individual_at(cursor);
                if (target != NULL) {
                    publish_event(Event::beam_of_striking_hit_individual(target), &perceived_current_zapper);
                    strike_individual(wand_wielder, target);
                    beam_length -= 3;
                }
                if (!is_open_space(actual_map_tiles[cursor].tile_type)) {
                    publish_event(Event::beam_hit_wall_no_effect(cursor));
                    beam_length = i;
                }
                break;
            }
            case WandId_WAND_OF_CONFUSION: {
                Thing target = find_individual_at(cursor);
                if (target != NULL) {
                    confuse_individual_from_wand(target, &perceived_current_zapper);
                    beam_length -= 3;
                }
                if (!is_open_space(actual_map_tiles[cursor].tile_type)) {
                    publish_event(Event::beam_hit_wall_no_effect(cursor));
                    beam_length = i;
                }
                break;
            }
            case WandId_WAND_OF_SPEED: {
                Thing target = find_individual_at(cursor);
                if (target != NULL) {
                    speed_up_individual(target);
                    publish_event(Event::beam_of_speed_hit_individual(target), &perceived_current_zapper);
                    beam_length -= 3;
                }
                if (!is_open_space(actual_map_tiles[cursor].tile_type)) {
                    publish_event(Event::beam_hit_wall_no_effect(cursor));
                    beam_length = i;
                }
                break;
            }
            case WandId_COUNT:
            case WandId_UNKNOWN:
                panic("not a real wand id");
        }
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
    actual_things.remove(item->id);
    fix_z_orders(actor->id);

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

    switch (wand_id) {
        case WandId_WAND_OF_CONFUSION:
            for (int i = 0; i < affected_individuals.length(); i++) {
                Thing target = affected_individuals[i];
                if (target->life()->species()->has_mind) {
                    publish_event(Event::explosion_of_confusion_hit_individual(target), &perceived_current_zapper);
                    confuse_individual(target);
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
                publish_event(Event::beam_of_digging_hit_wall(wall_location), &perceived_current_zapper);
                change_map(wall_location, TileType_FLOOR);
            }
            break;
        case WandId_WAND_OF_STRIKING:
            for (int i = 0; i < affected_individuals.length(); i++) {
                Thing target = affected_individuals[i];
                publish_event(Event::explosion_of_striking_hit_individual(target), &perceived_current_zapper);
                strike_individual(actor, target);
            }
            for (int i = 0; i < affected_walls.length(); i++)
                publish_event(Event::explosion_hit_wall_no_effect(affected_walls[i]), &perceived_current_zapper);
            break;
        case WandId_WAND_OF_SPEED:
            for (int i = 0; i < affected_individuals.length(); i++) {
                Thing target = affected_individuals[i];
                speed_up_individual(target);
                publish_event(Event::explosion_of_speed_hit_individual(target), &perceived_current_zapper);
            }
            for (int i = 0; i < affected_walls.length(); i++)
                publish_event(Event::explosion_hit_wall_no_effect(affected_walls[i]), &perceived_current_zapper);
            break;

        case WandId_COUNT:
        case WandId_UNKNOWN:
            panic("not a real wand id");
    }
}
