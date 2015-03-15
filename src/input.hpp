#ifndef INPUT_HPP
#define INPUT_HPP

#include "geometry.hpp"
#include "hashtable.hpp"

enum InputMode {
    InputMode_MAIN,
    InputMode_DROP_CHOOSE_ITEM,
    InputMode_THROW_CHOOSE_ITEM,
    InputMode_THROW_CHOOSE_DIRECTION,
    InputMode_QUAFF_CHOOSE_ITEM,
    InputMode_ZAP_CHOOSE_ITEM,
    InputMode_ZAP_CHOOSE_DIRECTION,
};

extern bool request_shutdown;

bool input_mode_is_choose_item();
bool input_mode_is_choose_direction();
extern int inventory_cursor;
extern InputMode input_mode;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();

#endif
