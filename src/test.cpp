
#include "string.hpp"

#define fail(msg) puke(__FILE__, __LINE__, __func__, msg);
static void puke(const char * filename, int line_number, const char * function_name, const char * msg) {
    fprintf(stderr, "%s:%d: %s: %s\n", filename, line_number, function_name, msg);
    abort();
}

static void test_string_format() {
    String string = new_string();
    string->format("%d", 4);
    string->format("%s", string);
    if (*string != *new_string("44"))
        fail("wrong string value");
}

int main() {
    test_string_format();

    return 0;
}
