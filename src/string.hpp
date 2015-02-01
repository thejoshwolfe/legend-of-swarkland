#ifndef STRING_HPP
#define STRING_HPP

struct CharBuffer {
    char * chars;
    int size;
    int reference_count;
    CharBuffer(char * chars, int size) :
            chars(chars), size(size), reference_count(0) {
    }
};

class String {
public:
    String(const String & copy);
    String(const char * str);
    ~String();
    int size() const {
        return _end - _start;
    }
    void copy(char * dest, int offset, int end) const;
    // concatenation
    String operator+(String other);
private:
    String(CharBuffer * char_buffer, int start, int end);
    CharBuffer * _char_buffer;
    int _start;
    int _end;
};

String toString(int n);

#endif
