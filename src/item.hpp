#ifndef ITEM_HPP
#define ITEM_HPP

#include "hashtable.hpp"
#include "reference_counter.hpp"
#include "geometry.hpp"

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

struct ItemImpl : public ReferenceCounted {
    uint256 id;
    WandDescriptionId description_id;
    Coord floor_location = Coord::nowhere();
    uint256 owner_id = uint256::zero();
    int z_order = 0;
    int charges;
    ItemImpl(uint256 id, WandDescriptionId description_id, int charges) :
            id(id), description_id(description_id), charges(charges) {
    }
};
typedef Reference<ItemImpl> Item;


void init_items();
Item random_item();

#endif
