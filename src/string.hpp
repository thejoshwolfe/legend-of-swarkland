#ifndef STRING_HPP
#define STRING_HPP

#include "list.hpp"
#include "byte_buffer.hpp"
#include "reference_counter.hpp"

#include <stdint.h>

static inline int find_percent_something(const char * fmt, char expected_code) {
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%')
            continue;
        i++;
        if (fmt[i] != expected_code)
            panic("wrong format");
        i++;
        return i;
    }
    if (expected_code == '\0')
        return -1;
    panic("too few function parameters");
}

// mutable unicode string.
// characters are effectively 21-bit integers.

class StringImpl;
typedef Reference<StringImpl> String;

class StringImpl : public ReferenceCounted {
public:
    StringImpl() {}

    // appends characters to this string
    void decode(const ByteBuffer &bytes, bool *ok);
    void decode(const ByteBuffer &bytes, int start, int end, bool *ok);
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

    // any char * is decoded as UTF-8
    void format(const char * fmt) {
        // check for too many %s
        find_percent_something(fmt, '\0');
        append(fmt);
    }
    template<typename... T>
    void format(const char * fmt, String s1, T... args);
    template<typename... T>
    void format(const char * fmt, const char * s1, T... args);
    template<typename... T>
    void format(const char * fmt, int d, T... args);

    void make_lower_case();
    void make_upper_case();

    static const int max_codepoint = 0x1fffff;
    void append(uint32_t c) {
        if (c > max_codepoint)
            panic("codepoint out of range");
        _chars.append(c);
    }
    // decoded as UTF-8
    void append(const char * str) {
        ByteBuffer buffer;
        buffer.append(str);
        decode(buffer);
    }
    void append(String s) {
        _chars.append_all(s->_chars);
    }

    void clear() {
        _chars.clear();
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

    // this or &other can be null
    bool operator==(const StringImpl & other) const {
        if (this == &other)
            return true;
        if (this == nullptr || &other == nullptr)
            return false;
        return _chars == other._chars;
    }
    bool operator!=(const StringImpl & other) const {
        return !(*this == other);
    }

private:
    List<uint32_t> _chars;

    StringImpl(const StringImpl &copy) = delete;
    StringImpl& operator= (const StringImpl& other) = delete;
};

template<typename... T>
void StringImpl::format(const char * fmt, String s1, T... args) {
    int i = find_percent_something(fmt, 's');
    ByteBuffer prefix;
    prefix.append(fmt, i - 2);
    decode(prefix);

    append(s1);

    format(fmt + i, args...);
}
template<typename... T>
void StringImpl::format(const char * fmt, const char * s1, T... args) {
    int i = find_percent_something(fmt, 's');
    ByteBuffer prefix;
    prefix.append(fmt, i - 2);
    decode(prefix);

    append(s1);

    format(fmt + i, args...);
}

template<typename... T>
void StringImpl::format(const char * fmt, int d, T... args) {
    int i = find_percent_something(fmt, 'd');
    ByteBuffer prefix;
    prefix.append(fmt, i - 2);
    decode(prefix);

    char buffer[64];
    sprintf(buffer, "%d", d);
    ByteBuffer bytes;
    bytes.append(buffer);
    decode(bytes);

    format(fmt + i, args...);
}

static inline String new_string() {
    return create<StringImpl>();
}
static inline String new_string(const ByteBuffer & buffer) {
    String result = new_string();
    result->decode(buffer);
    return result;
}
static inline String new_string(const char * str) {
    ByteBuffer buffer;
    buffer.append(str);
    return new_string(buffer);
}

static inline void fprintf_string(FILE * stream, String string) {
    ByteBuffer buffer;
    string->encode(&buffer);
    fprintf(stream, "%s", buffer.raw());
}

#endif
