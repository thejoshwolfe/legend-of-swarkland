#ifndef INDIVIDUAL_H
#define INDIVIDUAL_H

#include "geometry.hpp"

#include <stdbool.h>

typedef struct {
    int hitpoints;
    int damage;
    Coord location;
    bool is_alive;
    bool is_ai;
} Individual;

#endif
