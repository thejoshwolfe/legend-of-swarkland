#ifndef STRING_HPP
#define STRING_HPP

#include "list.hpp"
#include "byte_buffer.hpp"
#include "reference_counter.hpp"

#include <stdint.h>

// mutable unicode string.
// characters are effectively 21-bit integers.

class StringImpl;
typedef Reference<StringImpl> String;

class StringImpl : public ReferenceCounted {
public:
    StringImpl() {}

    StringImpl(const StringImpl &copy) = delete;
    StringImpl& operator= (const StringImpl& other) = delete;

    // appends characters to this string
    void decode(const ByteBuffer &bytes, bool *ok);
    // this one panics if the string is invalid
    void decode(const ByteBuffer &bytes);
    void encode(ByteBuffer * output_bytes) const;

    int length() const {
        return _chars.length();
    }
    const uint32_t & operator[](int index) const {
        return _chars[index];
    }
    uint32_t & operator[](int index) {
        return _chars[index];
    }

    void make_lower_case();
    void make_upper_case();

    static const int max_codepoint = 0x1fffff;
    void append(uint32_t c) {
        if (c > max_codepoint)
            panic("codepoint out of range");
        _chars.append(c);
    }

    String substring(int start, int end) const;
    String substring(int start) const;

    void replace(int start, int end, String s);

    void split_on_whitespace(List<StringImpl> &out) const;

    int index_of_insensitive(const StringImpl &other) const;

    static int compare(const StringImpl &a, const StringImpl &b);
    static int compare_insensitive(const StringImpl &a, const StringImpl &b);
    static uint32_t char_to_lower(uint32_t c);
    static uint32_t char_to_upper(uint32_t c);

    static bool is_whitespace(uint32_t c);

private:
    List<uint32_t> _chars;
};

#endif
