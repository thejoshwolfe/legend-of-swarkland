#include "byte_buffer.hpp"

#include "util.hpp"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>

ByteBuffer::ByteBuffer() {
    _buffer.append(0);
}

void ByteBuffer::append(const ByteBuffer & other) {
    append(other.raw(), other.length());
}

void ByteBuffer::append(const char * str) {
    append(str, strlen(str));
}

void ByteBuffer::append(const char * str, int length) {
    int prev_length_plus_null = _buffer.length();
    int new_length_plus_null = prev_length_plus_null + length;
    _buffer.resize(new_length_plus_null);
    memcpy(_buffer.raw() + prev_length_plus_null - 1, str, length);
    _buffer[new_length_plus_null - 1] = 0;
}

void ByteBuffer::format(const char *format, ...) {
    va_list ap, ap2;
    va_start(ap, format);
    va_copy(ap2, ap);

    int ret = vsnprintf(NULL, 0, format, ap);
    if (ret < 0)
        panic("vsnprintf error");

    int start_index = length();
    int required_length = ret + 1;
    _buffer.resize(start_index + required_length);

    ret = vsnprintf(_buffer.raw() + start_index, required_length, format, ap2);
    if (ret < 0)
        panic("vsnprintf error 2");

    va_end(ap2);
    va_end(ap);
}

int ByteBuffer::index_of_rev(char c) const {
    return index_of_rev(c, length() - 1);
}

int ByteBuffer::index_of_rev(char c, int start) const {
    for (int i = start; i >= 0; i -= 1) {
        if (_buffer[i] == c)
            return i;
    }
    return -1;
}
