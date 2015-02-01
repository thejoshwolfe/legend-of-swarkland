#include "util.hpp"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

void panic(const char * str) {
    fprintf(stderr, "%s\n", str);
    abort();
}

void init_random() {
    srand(time(NULL));
}

int random_int(int less_than_this) {
    return rand() % less_than_this;
}
