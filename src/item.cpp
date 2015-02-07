#include "item.hpp"

#include "individual.hpp"
#include "util.hpp"

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

void get_item_description(Individual observer, Item item, ByteBuffer * output) {
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

