#include "tas.hpp"

#include <stdio.h>
#include <time.h>

static const uint256 magic_number = {{
        0x9a0f331969e3299cULL,
        0x198678e2bb49e5acULL,
        0xa6e8b4fc66b28fe5ULL,
        0x9ba16cf305797cbeULL,
}};

struct TasHeader {
    uint256 magic;
    uint32_t seed;
};

static FILE * output_script_file;
static FILE * input_script_file;
static uint32_t tas_seed;

void tas_set_output_script(char * filename) {
    output_script_file = fopen(filename, "wb");
    tas_seed = time(NULL);
    TasHeader header = {magic_number, tas_seed};
    fwrite(&header, sizeof(TasHeader), 1, output_script_file);
    fflush(output_script_file);
}
void tas_set_input_script(char * filename) {
    input_script_file = fopen(filename, "rb");
    TasHeader header;
    fread(&header, sizeof(TasHeader), 1, input_script_file);
    if (header.magic != magic_number)
        panic("tas input script magic number mismatch");
    tas_seed = header.seed;
}

Action tas_get_decision() {
    if (input_script_file == NULL)
        return Action::undecided(); // no, you decide.

    Action result;
    if (fread(&result, sizeof(Action), 1, input_script_file) < 1) {
        // end of script
        return Action::undecided();
    }
    if (result == Action::undecided())
        panic("read indecision from input script");
    return result;
}

void tas_record_decision(Action action) {
    if (output_script_file != NULL) {
        fwrite(&action, sizeof(Action), 1, output_script_file);
        fflush(output_script_file);
    }
}

uint32_t tas_get_seed() {
    if (input_script_file == NULL)
        tas_seed = time(NULL);
    return tas_seed;
}
