#include "item.hpp"

#include "individual.hpp"
#include "util.hpp"
#include "swarkland.hpp"
#include "event.hpp"

WandId actual_wand_identities[WandId_COUNT];

void init_items() {
    for (int i = 0; i < WandId_COUNT; i++)
        actual_wand_identities[i] = (WandId)i;
    // shuffle
    for (int i = 0; i < WandId_COUNT - 1; i++) {
        int swap_with = random_int(i, WandId_COUNT);
        WandId tmp = actual_wand_identities[swap_with];
        actual_wand_identities[swap_with] = actual_wand_identities[i];
        actual_wand_identities[i] = tmp;
    }
}

Item random_item() {
    uint256 id = random_uint256();
    WandDescriptionId description_id = (WandDescriptionId)random_int(WandDescriptionId_COUNT);
    int charges = random_int(4, 8);
    Item item = new ItemImpl(id, description_id, charges);
    actual_items.put(id, item);
    return item;
}

void get_item_description(Individual observer, uint256 wielder_id, uint256 item_id, ByteBuffer * output) {
    Item item = actual_items.get(item_id);
    if (!can_see_individual(observer, wielder_id)) {
        // can't see the wand
        output->append("a wand");
        return;
    }
    WandId true_id = observer->knowledge.wand_identities[item->description_id];
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
        switch (item->description_id) {
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

static void confuse_individual_from_wand(Individual target) {
    bool did_it_work = confuse_individual(target);
    if (did_it_work) {
        publish_event(Event::beam_of_confusion_hit_individual(target));
    } else {
        publish_event(Event::beam_hit_individual_no_effect(target));
    }
}

static void strike_individual_from_wand(Individual wand_wielder, Individual target) {
    publish_event(Event::beam_of_striking_hit_individual(target));
    strike_individual(wand_wielder, target);
}

void zap_wand(Individual wand_wielder, uint256 item_id, Coord direction) {
    Item wand = actual_items.get(item_id);
    wand->charges--;
    if (wand->charges <= -1) {
        publish_event(Event::wand_disintegrates(wand_wielder, wand));
        // uh... shift everything down 1, and resize out 1 item.
        for (int i = 0; i < wand_wielder->inventory.length() - 1; i++)
            wand_wielder->inventory[i] = wand_wielder->inventory[i + 1];
        wand_wielder->inventory.resize(wand_wielder->inventory.length() - 1);

        actual_items.remove(item_id);
        return;
    }
    if (wand->charges <= 0) {
        publish_event(Event::wand_zap_no_charges(wand_wielder, wand));
        return;
    }

    publish_event(Event::zap_wand(wand_wielder, wand));
    Coord cursor = wand_wielder->location;
    int beam_length = random_int(6, 13);
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;
        switch (actual_wand_identities[wand->description_id]) {
            case WandId_WAND_OF_DIGGING: {
                if (actual_map_tiles[cursor].tile_type == TileType_WALL) {
                    change_map(cursor, TileType_FLOOR);
                    publish_event(Event::beam_of_digging_hit_wall(cursor));
                } else {
                    // the digging beam doesn't travel well through air
                    beam_length -= 3;
                }
                break;
            }
            case WandId_WAND_OF_STRIKING: {
                Individual target = find_individual_at(cursor);
                if (target != NULL) {
                    strike_individual_from_wand(wand_wielder, target);
                    beam_length -= 3;
                }
                if (actual_map_tiles[cursor].tile_type == TileType_WALL)
                    beam_length = i;
                break;
            }
            case WandId_WAND_OF_CONFUSION: {
                Individual target = find_individual_at(cursor);
                if (target != NULL) {
                    confuse_individual_from_wand(target);
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

    // the zap has stopped
    for (auto iterator = actual_individuals.value_iterator(); iterator.has_next();) {
        iterator.next()->knowledge.wand_being_zapped = {
            NULL,
            NULL,
        };
    }
}
