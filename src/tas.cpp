#include "tas.hpp"

#include <stdio.h>
#include <time.h>

int tas_delay;

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
    output_script_file = (strcmp(filename, "-") == 0) ? stdout : fopen(filename, "wb");
    tas_seed = time(nullptr);
    TasHeader header = {magic_number, tas_seed};
    fwrite(&header, sizeof(TasHeader), 1, output_script_file);
    fflush(output_script_file);
}
void tas_set_input_script(char * filename) {
    input_script_file = (strcmp(filename, "-") == 0) ? stdin : fopen(filename, "rb");
    TasHeader header;
    int elements_read = fread(&header, sizeof(TasHeader), 1, input_script_file);
    if (elements_read != 1 || header.magic != magic_number)
        panic("tas input script magic number mismatch");
    tas_seed = header.seed;
}

int frame_counter;

Action tas_get_decision() {
    if (tas_delay > 0) {
        if (frame_counter < tas_delay) {
            frame_counter++;
            return Action::undecided(); // let the screen draw
        }
        frame_counter = 0;
    }
    if (input_script_file == nullptr)
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
    if (output_script_file != nullptr) {
        fwrite(&action, sizeof(Action), 1, output_script_file);
        fflush(output_script_file);
    }
}

uint32_t tas_get_seed() {
    if (input_script_file == nullptr)
        tas_seed = time(nullptr);
    return tas_seed;
}
