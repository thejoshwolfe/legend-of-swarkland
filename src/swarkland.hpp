#ifndef SWARKLAND_HPP
#define SWARKLAND_HPP

#include "geometry.hpp"
#include "list.hpp"
#include "individual.hpp"

static const Coord map_size = { 55, 30 };
extern Species * specieses[SpeciesId_COUNT];
extern List<Individual *> individuals;
extern Individual * you;
extern long long time_counter;

void swarkland_init();
void move_with_ai(Individual * individual);
void you_move(Coord delta);

#endif
