#ifndef ITEM_HPP
#define ITEM_HPP

#include "individual.hpp"

const int beam_length_average = 9;
const int beam_length_error_margin = 3;

void init_items();
Thing random_item();
void zap_wand(Thing individual, uint256 item_id, Coord direction);
void explode_wand(Thing actor, Thing item, Coord explosion_center);

#endif
