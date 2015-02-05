#include "string.hpp"

#include "util.hpp"

#include <string.h>
#include <stdio.h>

String::String(const char * str) {
    int size = strlen(str);
    char * chars = new char[size];
    memcpy(chars, str, size);
    _char_buffer = new CharBuffer(chars, size);
    _char_buffer->reference_count++;
    _start = 0;
    _end = size;
}
String::String(const String & copy) {
    if (&copy == this)
        return;
    _char_buffer = copy._char_buffer;
    _char_buffer->reference_count++;
    _start = copy._start;
    _end = copy._end;
}
String::String(CharBuffer * char_buffer, int start, int end) {
    _char_buffer = char_buffer;
    _char_buffer->reference_count++;
    _start = start;
    _end = end;
}

String::~String() {
    _char_buffer->reference_count--;
    if (_char_buffer->reference_count == 0) {
        delete[] _char_buffer->chars;
        delete _char_buffer;
    }
}
void String::copy(char * dest, int offset, int end) const {
    if (offset < 0 || offset >= size())
        panic("string bounds");
    if (end < offset || end > size())
        panic("string bounds");
    memcpy(dest, _char_buffer->chars + _start + offset, end - offset);
}

String String::operator+(String other) {
    int new_size = this->size() + other.size();
    char * new_chars = new char[new_size];
    this->copy(new_chars, 0, this->size());
    other.copy(new_chars + this->size(), 0, other.size());
    return String(new CharBuffer(new_chars, new_size), 0, new_size);
}

String to_string(int n) {
    char buffer[256];
    sprintf(buffer, "%d", n);
    return String(buffer);
}

