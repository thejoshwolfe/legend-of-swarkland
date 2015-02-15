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
    actual_items.put(item->id, item);
    return item;
}

void get_item_description(Thing observer, uint256 wielder_id, uint256 item_id, ByteBuffer * output) {
    Thing item = actual_items.get(item_id);
    if (!can_see_individual(observer, wielder_id)) {
        // can't see the wand
        output->append("a wand");
        return;
    }
    WandId true_id = observer->life()->knowledge.wand_identities[item->wand_info()->description_id];
    if (true_id != WandId_UNKNOWN) {
        switch (true_id) {
            case WandId_WAND_OF_CONFUSION:
                output->append("a wand of confusion");
                return;
            case WandId_WAND_OF_DIGGING:
                output->append("a wand of digging");
                return;
            case WandId_WAND_OF_STRIKING:
                output->append("a wand of striking");
                return;
            default:
                panic("wand id");
        }
    } else {
        switch (item->wand_info()->description_id) {
            case WandDescriptionId_BONE_WAND:
                output->append("a bone wand");
                return;
            case WandDescriptionId_GOLD_WAND:
                output->append("a gold wand");
                return;
            case WandDescriptionId_PLASTIC_WAND:
                output->append("a plastic wand");
                return;
            default:
                panic("wand id");
        }
    }
}

static void confuse_individual_from_wand(Thing target, IdMap<WandDescriptionId> * perceived_current_zapper) {
    bool did_it_work = confuse_individual(target);
    if (did_it_work) {
        publish_event(Event::beam_of_confusion_hit_individual(target), perceived_current_zapper);
    } else {
        publish_event(Event::beam_hit_individual_no_effect(target), perceived_current_zapper);
    }
}

static void strike_individual_from_wand(Thing wand_wielder, Thing target, IdMap<WandDescriptionId> * perceived_current_zapper) {
    publish_event(Event::beam_of_striking_hit_individual(target), perceived_current_zapper);
    strike_individual(wand_wielder, target);
}

void zap_wand(Thing wand_wielder, uint256 item_id, Coord direction) {
    IdMap<WandDescriptionId> perceived_current_zapper;

    Thing wand = actual_items.get(item_id);
    wand->wand_info()->charges--;
    if (wand->wand_info()->charges <= -1) {
        publish_event(Event::wand_disintegrates(wand_wielder, wand), &perceived_current_zapper);

        actual_items.remove(item_id);
        // reassign z orders
        List<Thing> inventory;
        find_items_in_inventory(wand_wielder, &inventory);
        for (int i = 0; i < inventory.length(); i++)
            inventory[i]->z_order = i;

        return;
    }
    if (wand->wand_info()->charges <= 0) {
        publish_event(Event::wand_zap_no_charges(wand_wielder, wand), &perceived_current_zapper);
        return;
    }

    publish_event(Event::zap_wand(wand_wielder, wand), &perceived_current_zapper);
    Coord cursor = wand_wielder->location;
    int beam_length = random_int(6, 13);
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;
        switch (actual_wand_identities[wand->wand_info()->description_id]) {
            case WandId_WAND_OF_DIGGING: {
                if (actual_map_tiles[cursor].tile_type == TileType_WALL) {
                    change_map(cursor, TileType_FLOOR);
                    publish_event(Event::beam_of_digging_hit_wall(cursor), &perceived_current_zapper);
                } else {
                    // the digging beam doesn't travel well through air
                    beam_length -= 3;
                }
                break;
            }
            case WandId_WAND_OF_STRIKING: {
                Thing target = find_individual_at(cursor);
                if (target != NULL) {
                    strike_individual_from_wand(wand_wielder, target, &perceived_current_zapper);
                    beam_length -= 3;
                }
                if (actual_map_tiles[cursor].tile_type == TileType_WALL)
                    beam_length = i;
                break;
            }
            case WandId_WAND_OF_CONFUSION: {
                Thing target = find_individual_at(cursor);
                if (target != NULL) {
                    confuse_individual_from_wand(target, &perceived_current_zapper);
                    beam_length -= 3;
                }
                if (actual_map_tiles[cursor].tile_type == TileType_WALL)
                    beam_length = i;
                break;
            }
            default:
                panic("wand id");
        }
    }
}
