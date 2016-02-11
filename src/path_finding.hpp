#ifndef PATH_FINDING_HPP
#define PATH_FINDING_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "map.hpp"
#include "thing.hpp"

static constexpr Coord directions[8] = {
    // start with the cardinal directions, because these are more "direct"
    // TODO: the order shouldn't matter. path finding favors cardinal directions through other means.
    {-1,  0},
    { 0, -1},
    { 1,  0},
    { 0,  1},
    {-1, -1},
    { 1, -1},
    { 1,  1},
    {-1,  1},
};

bool do_i_think_i_can_move_here(Thing individual, Coord location);
bool find_path(Coord start, Coord end, Thing according_to_whom, List<Coord> * output_path);

#endif
