#include "tas.hpp"

#include "random.hpp"

#include <stdio.h>
#include <errno.h>

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

static FILE * script_file;
static uint32_t tas_seed;
static int frame_counter;
static TasScriptMode current_mode;

void set_tas_delay(int n) {
    tas_delay = n;
}
uint32_t tas_get_seed() {
    return tas_seed;
}

void set_tas_script(TasScriptMode mode, const char * file_path) {
    switch (mode) {
        case TasScriptMode_WRITE:
            script_file = fopen(file_path, "wb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not create file: %s\n", file_path);
                exit(1);
            }
            current_mode = TasScriptMode_WRITE;
            break;
        case TasScriptMode_READ:
            script_file = fopen(file_path, "rb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not read file: %s\n", file_path);
                exit(1);
            }
            current_mode = TasScriptMode_READ;
            break;
        case TasScriptMode_READ_WRITE:
            script_file = fopen(file_path, "r+b");
            if (script_file != nullptr) {
                // first read, then write
                current_mode = TasScriptMode_READ_WRITE;
            } else {
                if (errno != ENOENT) {
                    fprintf(stderr, "ERROR: could not read/create file: %s\n", file_path);
                    exit(1);
                }
                // no problem. we'll just make it
                script_file = fopen(file_path, "wb");
                if (script_file == nullptr) {
                    fprintf(stderr, "ERROR: could not create file: %s\n", file_path);
                    exit(1);
                }
                current_mode = TasScriptMode_WRITE;
            }
            break;
        case TasScriptMode_IGNORE:
            current_mode = TasScriptMode_IGNORE;
            break;
    }

    switch (current_mode) {
        case TasScriptMode_READ:
        case TasScriptMode_READ_WRITE: {
            TasHeader header;
            int elements_read = fread(&header, sizeof(TasHeader), 1, script_file);
            if (elements_read != 1 || header.magic != magic_number)
                panic("tas input script magic number mismatch");
            tas_seed = header.seed;
            break;
        }
        case TasScriptMode_WRITE: {
            tas_seed = get_random_seed();
            TasHeader header = {magic_number, tas_seed};
            fwrite(&header, sizeof(TasHeader), 1, script_file);
            fflush(script_file);
            break;
        }
        case TasScriptMode_IGNORE:
            tas_seed = get_random_seed();
            break;
    }
}

static Action read_action() {
    Action result;
    if (fread(&result, sizeof(Action), 1, script_file) < 1) {
        // end of script
        return Action::undecided();
    }
    if (result == Action::undecided())
        panic("read indecision from input script");
    return result;
}

Action tas_get_decision() {
    if (tas_delay > 0) {
        if (frame_counter < tas_delay) {
            frame_counter++;
            return Action::undecided(); // let the screen draw
        }
        frame_counter = 0;
    }
    switch (current_mode) {
        case TasScriptMode_READ_WRITE: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                current_mode = TasScriptMode_WRITE;
            }
            return result;
        }
        case TasScriptMode_READ: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                fclose(script_file);
                current_mode = TasScriptMode_IGNORE;
            }
            return result;
        }
        case TasScriptMode_WRITE:
        case TasScriptMode_IGNORE:
            // no, you decide.
            return Action::undecided();
    }
    unreachable();
}

void tas_record_decision(Action action) {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't write
            break;
        case TasScriptMode_WRITE:
            fwrite(&action, sizeof(Action), 1, script_file);
            fflush(script_file);
            break;
    }
}
