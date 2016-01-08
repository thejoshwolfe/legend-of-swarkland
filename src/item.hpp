#ifndef ITEM_HPP
#define ITEM_HPP

#include "thing.hpp"

const int beam_length_average = 9;
const int beam_length_error_margin = 3;

void init_items();
Thing create_random_item();
Thing create_random_item(ThingType thing_type);
Thing create_wand(WandId wand_id);
Thing create_potion(PotionId potion_id);
void delete_item(Thing item);
void zap_wand(Thing individual, uint256 item_id, Coord direction);
void use_potion(Thing actor, Thing target, Thing item, bool is_breaking);
void explode_wand(Thing actor, Thing item, Coord explosion_center);
void break_potion(Thing actor, Thing item, Coord location);

#endif
