#ifndef BYTE_BUFFER
#define BYTE_BUFFER

#include "list.hpp"
#include <stdio.h>
#include <string.h>

static int constexpr strlen_constexpr(char const* str) {
#if __cpp_constexpr >= 201304
    // we prefer not to rely on strlen() being magically allowed to be constexpr by some compilers
    int i = 0;
    while (str[i] != '\0')
        i++;
    return i;
#else
    // ok, let's see if this works.
    return strlen(str);
#endif
}

class ConstStr {
public:
    char const* const ptr;
    int const len;

    constexpr ConstStr(decltype(nullptr)) :
        ptr(nullptr), len(-1) {
    }
    constexpr ConstStr(char const* str) :
        ptr(str), len(strlen_constexpr(str)) {
    }
};

class ByteBuffer {
public:
    ByteBuffer();
    ~ByteBuffer() {}

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
    void remove_range(int start, int end) {
        // don't delete the null terminator
        assert(end <= length());
        _buffer.remove_range(start, end);
    }
    void append(const ByteBuffer & other) {
        append(other.raw(), other.length());
    }
    void append(const char * str);
    void append(const char * str, int length);
    void append(ConstStr str) {
        append(str.ptr, str.len);
    }
    void append(char c) {
        _buffer[length()] = c;
        _buffer.append(0);
    }

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

    inline bool operator==(ByteBuffer const& other) const {
        return this->_buffer == other._buffer;
    }
    inline bool operator!=(ByteBuffer const& other) const {
        return !(*this == other);
    }

private:
    List<char> _buffer;

    ByteBuffer(const ByteBuffer & copy) = delete;
    ByteBuffer & operator=(const ByteBuffer & other) = delete;
};

DEFINE_GDB_PY_SCRIPT("debug-scripts/byte_buffer.py")

#endif
