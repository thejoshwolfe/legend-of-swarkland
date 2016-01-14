#ifndef TAS_HPP
#define TAS_HPP

#include "swarkland.hpp"

enum TasScriptMode {
    TasScriptMode_WRITE,
    TasScriptMode_READ_WRITE,
    TasScriptMode_READ,
    TasScriptMode_IGNORE,
};

void set_tas_script(TasScriptMode mode, const char * file_path);
void set_tas_delay(int n);
uint32_t tas_get_seed();
Action tas_get_decision();
void tas_record_decision(Action action);
void tas_delete_save();

#endif
