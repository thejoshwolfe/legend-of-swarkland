#ifndef STRING_HPP
#define STRING_HPP

#include "list.hpp"
#include "byte_buffer.hpp"

#include <stdint.h>

// UTF-8 string
class String {
public:
    String() {}

    String(const String & copy) = delete;
    String & operator=(const String & other) = delete;

    // append to this object. panic on decode error
    void decode(const ByteBuffer & bytes) {
        bool ok;
        decode(bytes, &ok);
        if (!ok)
            panic("invalid UTF-8");
    }
    // append to this object.
    void decode(const ByteBuffer & bytes, bool * output_ok);

    // append to the buffer
    void encode(ByteBuffer * output_bytes) const;

    int length() const {
        return _chars.length();
    }
    const uint32_t & at(int index) const {
        return _chars[index];
    }
    uint32_t & at(int index) {
        return _chars[index];
    }

    static const int max_codepoint = 0x1fffff;
    void append(uint32_t c) {
        if (c > max_codepoint)
            panic("codepoint out of range");
        _chars.append(c);
    }

private:
    List<uint32_t> _chars;
};

#endif
