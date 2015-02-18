#ifndef INPUT_HPP
#define INPUT_HPP

#include "geometry.hpp"
#include "hashtable.hpp"

extern bool request_shutdown;

enum InputMode {
    InputMode_MAIN,
    InputMode_DROP_CHOOSE_ITEM,
    InputMode_THROW_CHOOSE_ITEM,
    InputMode_THROW_CHOOSE_DIRECTION,
    InputMode_ZAP_CHOOSE_ITEM,
    InputMode_ZAP_CHOOSE_DIRECTION,
};
extern InputMode input_mode;
extern uint256 chosen_item;
extern int inventory_cursor;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();

#endif
