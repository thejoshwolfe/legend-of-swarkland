
#include "string.hpp"

#define fail(msg) puke(__FILE__, __LINE__, __func__, msg);
static void puke(const char * filename, int line_number, const char * function_name, const char * msg) {
    // gcc compile error format, so that eclipse will give us red squiggly underlines where the fail was called.
    fprintf(stderr, "%s:%d: %s: %s\n", filename, line_number, function_name, msg);
    abort();
}

static void test_list() {
    {
        List<int> list;
        for (int i = 0; i < 15; i++)
            list.append(i);
        list.remove_range(1, 14);
        if (!(list[0] == 0 && list[1] == 14))
            fail("shift indexes");
        if (list.length() != 2)
            fail("should have deleted everything");
    }
    {
        List<int> list;
        list.remove_range(0, 0);
        list.append(1234);
        list.remove_range(0, 0);
        list.remove_range(1, 1);
        if (list[0] != 1234)
            fail("remove_range should work for empty list");
    }
}

static void test_string_format() {
    String string = new_string();
    string->format("%d", 4);
    string->format("%s", string);
    if (*string != *new_string("44"))
        fail("wrong string value");
}

int main() {
    test_list();
    test_string_format();

    return 0;
}
