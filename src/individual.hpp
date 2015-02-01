#ifndef INDIVIDUAL_HPP
#define INDIVIDUAL_HPP

#include "geometry.hpp"

#include <stdbool.h>

struct Individual {
    int hitpoints;
    int damage;
    Coord location;
    bool is_alive;
    bool is_ai;
    Individual(int hitpoints, int damage, Coord location, bool is_alive, bool is_ai) :
            hitpoints(hitpoints), damage(damage), location(location), is_alive(is_alive), is_ai(is_ai) {
    }
};

#endif
