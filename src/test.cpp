
#include "string.hpp"

static void test_string_format() {
    fprintf(stdout, "string formatting: ");
    String string = new_string();
    string->format("%d", 4);
    string->format("%s", string);
    fprintf_string(stdout, string);
    if (*string != *new_string("44"))
        panic("\nwrong string value");
    fprintf(stdout, ": pass");
}

int main() {
    test_string_format();
    return 0;
}
