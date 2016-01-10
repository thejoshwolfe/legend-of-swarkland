#ifndef TAS_HPP
#define TAS_HPP

#include "swarkland.hpp"

Action tas_get_decision();
void tas_record_decision(Action action);
uint32_t tas_get_seed();
void tas_set_output_script(char * filename);
void tas_set_input_script(char * filename);

extern int tas_delay;

#endif
