#ifndef BYTE_BUFFER
#define BYTE_BUFFER

#include "list.hpp"
#include <stdio.h>
#include <string.h>

class ByteBuffer {
public:
    ByteBuffer();
    ~ByteBuffer() {}

    ByteBuffer(const ByteBuffer & copy) = delete;
    ByteBuffer & operator=(const ByteBuffer & other) = delete;

    const char & operator[](int index) const {
        return _buffer[index];
    }
    char & operator[](int index) {
        return _buffer[index];
    }

    // appends formatted string
    void format(const char *format, ...) __attribute__ ((format (printf, 2, 3)));

    int length() const {
        return _buffer.length() - 1;
    }
    void resize(int new_length) {
        _buffer.resize(new_length + 1);
        _buffer[length()] = 0;
    }
    void append(const ByteBuffer & other);
    void append(const char * str);
    void append(const char * str, int length);

    int index_of_rev(char c) const;
    int index_of_rev(char c, int start) const;

    char * raw() {
        return _buffer.raw();
    }
    const char * raw() const {
        return _buffer.raw();
    }

    void fill(char value) {
        memset(_buffer.raw(), value, length());
    }
private:
    List<char> _buffer;
};

#endif
