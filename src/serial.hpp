#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "swarkland.hpp"

enum SaveFileMode {
    SaveFileMode_WRITE,
    SaveFileMode_READ_WRITE,
    SaveFileMode_READ,
    SaveFileMode_IGNORE,
};

extern bool replay_paused;
extern bool replay_advance;

void set_save_file(SaveFileMode mode, const char * file_path, bool cli_says_test_mode);
void set_replay_delay(int n);
void replay_adjust_delay(int delta);
void replay_resume();
void replay_pause();

void init_random();
Action read_decision_from_save_file();
void record_decision_to_save_file(const Action & action);
int read_rng_input_from_save_file(const ByteBuffer & tag);
void delete_save_file();


#endif
