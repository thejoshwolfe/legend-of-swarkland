
#include "string.hpp"

#define fail(msg) _fail(__FILE__, __LINE__, __func__, (msg))
static void _fail(const char * filename, int line_number, const char * function_name, const char * msg) {
    // gcc compile error format, so that eclipse will give us red squiggly underlines where the fail was called.
    fprintf(stderr, "%s:%d: %s: %s\n", filename, line_number, function_name, msg);
    abort();
}

#define assert(condition) _assert((condition), __FILE__, __LINE__, __func__)
static void _assert(bool condition, const char * filename, int line_number, const char * function_name) {
    if (!condition)
        _fail(filename, line_number, function_name, "assertion failure");
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

static void test_log2() {
    assert(log2(0) == 0);
    assert(log2(1) == 0);
    assert(log2(2) == 1);
    assert(log2(3) == 1);
    assert(log2(4) == 2);
    assert(log2(1023) == 9);
    assert(log2(1024) == 10);
    assert(log2(1025) == 10);
    assert(log2(0x40000000) == 30);
    assert(log2(0x7fffffff) == 30);
}

int main() {
    test_list();
    test_string_format();
    test_log2();

    return 0;
}
