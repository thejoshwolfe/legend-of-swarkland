#ifndef INPUT_HPP
#define INPUT_HPP

#include "geometry.hpp"

extern bool request_shutdown;

Coord get_mouse_pixels();
void on_mouse_motion();
void read_input();

#endif
