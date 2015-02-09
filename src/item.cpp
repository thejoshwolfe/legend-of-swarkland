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
    return {(WandDescriptionId)random_int(WandDescriptionId_COUNT)};
}

void get_item_description(Individual observer, Individual wielder, Item item, ByteBuffer * output) {
    if (!observer->knowledge.tile_is_visible[wielder->location].any()) {
        // can't see the wand
        output->append("a wand");
        return;
    }
    WandId true_id = observer->knowledge.wand_identities[item.description_id];
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
        switch (item.description_id) {
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

static void confuse_individual_from_wand(Individual wand_wielder, Item wand, Individual target) {
    bool did_it_work = confuse_individual(target);
    if (did_it_work) {
        publish_event(Event::wand_of_confusion_hit(wand_wielder, wand, target));
    } else {
        publish_event(Event::wand_hit_no_effect(wand_wielder, wand, target));
    }
}

static void strike_individual_from_wand(Individual wand_wielder, Item wand, Individual target) {
    publish_event(Event::wand_of_striking_hit(wand_wielder, wand, target));
    strike_individual(wand_wielder, target);
}

void zap_wand(Individual wand_wielder, Item wand, Coord direction) {
    publish_event(Event::zap_wand(wand_wielder, wand));
    Coord cursor = wand_wielder->location;
    int beam_length = random_int(6, 13);
    for (int i = 0; i < beam_length; i++) {
        cursor = cursor + direction;
        if (!is_in_bounds(cursor))
            break;
        switch (actual_wand_identities[wand.description_id]) {
            case WandId_WAND_OF_DIGGING:
            case WandId_WAND_OF_STRIKING: {
                Individual target = find_individual_at(cursor);
                if (target != NULL) {
                    strike_individual_from_wand(wand_wielder, wand, target);
                    beam_length -= 3;
                }
                if (actual_map_tiles[cursor].tile_type == TileType_WALL)
                    beam_length = i;
                break;
            }
            case WandId_WAND_OF_CONFUSION: {
                Individual target = find_individual_at(cursor);
                if (target != NULL) {
                    confuse_individual_from_wand(wand_wielder, wand, target);
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
