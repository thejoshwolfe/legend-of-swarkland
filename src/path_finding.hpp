#ifndef PATH_FINDING_HPP
#define PATH_FINDING_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "map.hpp"

bool find_path(Coord start, Coord end, Matrix<Tile> & tiles, List<Coord> & output_path);

#endif
