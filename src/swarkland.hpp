#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

static const Coord map_size = { 55, 30 };

struct Tile {
    bool is_open;
    bool is_visible;
    bool is_ever_seen;
    int aesthetic_index;
};
class Map {
public:
    Matrix<Tile> tiles;
    Map() :
            tiles(map_size.y, map_size.x) {
    }
};

extern Map the_map;

extern Species * specieses[SpeciesId_COUNT];

extern List<Individual *> individuals;
extern Individual * you;
extern long long time_counter;

extern bool cheatcode_full_visibility;
void cheatcode_kill_everybody_in_the_world();
void cheatcode_polymorph();

void swarkland_init();
void swarkland_finish();

void advance_time();
bool you_move(Coord delta);
Individual * find_individual_at(Coord location);

#endif
