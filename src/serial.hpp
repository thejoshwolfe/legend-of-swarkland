#ifndef SERIAL_HPP
#define SERIAL_HPP

#include "swarkland.hpp"

enum SaveFileMode {
    SaveFileMode_WRITE,
    SaveFileMode_READ_WRITE,
    SaveFileMode_READ,
    SaveFileMode_IGNORE,
};

void set_save_file(SaveFileMode mode, const char * file_path, bool cli_syas_test_mode);
void set_replay_delay(int n);
void init_random();
Action read_decision_from_save_file();
void record_decision_to_save_file(const Action & action);
int read_rng_input_from_save_file(const ByteBuffer & tag);
void delete_save_file();

__attribute__((noreturn))
void test_expect_fail(const char * message);
__attribute__((noreturn))
void test_expect_fail(const char * fmt, String s);
__attribute__((noreturn))
void test_expect_fail(const char * fmt, String s1, String s2);

#endif
