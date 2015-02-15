#ifndef PATH_FINDING_HPP
#define PATH_FINDING_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "map.hpp"
#include "individual.hpp"

extern const Coord directions[8];
bool do_i_think_i_can_move_here(Thing individual, Coord location);
bool find_path(Coord start, Coord end, Thing according_to_whom, List<Coord> & output_path);

#endif
