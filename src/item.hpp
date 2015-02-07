#ifndef ITEM_HPP
#define ITEM_HPP

enum WandDescriptionId {
    WandDescriptionId_BONE_WAND,
    WandDescriptionId_GOLD_WAND,
    WandDescriptionId_PLASTIC_WAND,

    WandDescriptionId_COUNT,
};
enum WandId {
    WandId_WAND_OF_CONFUSION,
    WandId_WAND_OF_DIGGING,
    WandId_WAND_OF_STRIKING,

    WandId_COUNT,
    WandId_UNKNOWN,
};

extern WandId actual_wand_identities[WandId_COUNT];

struct Item {
    WandDescriptionId description_id;
};


void init_items();
Item random_item();

#endif
