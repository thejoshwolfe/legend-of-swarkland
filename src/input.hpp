#ifndef INPUT_HPP
#define INPUT_HPP

#include "geometry.hpp"

extern bool request_shutdown;

enum InputMode {
    InputMode_MAIN,
    InputMode_ZAP_CHOOSE_DIRECTION,
};
extern InputMode input_mode;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();

#endif
