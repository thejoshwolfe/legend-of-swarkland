#ifndef TEXT_FILE_PARSER_HPP
#define TEXT_FILE_PARSER_HPP

#include "swarkland.hpp"

__attribute__((noreturn))
static void exit_with_error() {
    // put a breakpoint here
    exit(1);
}

struct Token {
    int start;
    int end;
};

class TextFileParser {
    // line_number starts at 1
    int line_number = 0;
    ByteBuffer read_buffer;
    const char* script_path;

    
    static bool read_line(ByteBuffer * output_buffer) {
        int index = 0;
        while (true) {
            // look for an EOL
            for (; index < read_buffer.length(); index++) {
                if (read_buffer[index] != '\n')
                    continue;
                line_number++;
                output_buffer->append(read_buffer.raw(), index);
                read_buffer.remove_range(0, index + 1);
                return true;
            }

            // read more chars
            char blob[256];
            size_t read_count = fread(blob, 1, 256, script_file);
            if (read_count == 0) {
                if (read_buffer.length() > 0) {
                    fprintf(stderr, "ERROR: expected newline at end of file: %s\n", script_path);
                    exit_with_error();
                }
                return false;
            }
            read_buffer.append(blob, read_count);
        }
    }

    static void tokenize_line(ByteBuffer const& line, List<Token> * tokens) {
        int index = 0;
        Token current_token = {0, 0};
        // col starts at 1
        int col = 0;
        for (; index < line.length(); index++) {
            uint32_t c = line[index];
            if (c == '#')
                break;
            if (col == 0) {
                if (c == ' ')
                    continue;
                // start token
                col = index + 1;
                if (c == '"') {
                    // start quoted string
                    while (true) {
                        index++;
                        if (index >= line.length()) {
                            fprintf(stderr, "%s:%d: error: unterminated quoted string\n", script_path, line_number);
                            exit_with_error();
                        }
                        c = line[index];
                        if (c == '"') {
                            // end quoted string
                            current_token.start = col - 1;
                            current_token.end = index + 1;
                            tokens->append(current_token);
                            current_token = Token{0, 0};
                            col = 0;
                            break;
                        }
                    }
                } else {
                    // start bareword token
                }
            } else {
                if (c != ' ')
                    continue;
                // end token
                current_token.start = col - 1;
                current_token.end = index;
                tokens->append(current_token);
                current_token = Token{0, 0};
                col = 0;
            }
        }
        if (col != 0) {
            // end token
            current_token.start = col - 1;
            current_token.end = index;
            tokens->append(current_token);
        }
    }
    static void read_tokens(ByteBuffer * output_line, List<Token> * output_tokens, bool tolerate_eof) {
        while (output_tokens->length() == 0) {
            if (!read_line(output_line)) {
                // EOF
                if (!tolerate_eof) {
                    fprintf(stderr, "%s:%d:1: error: unexpected EOF", script_path, line_number);
                    exit_with_error();
                }
                return;
            }
            tokenize_line(*output_line, output_tokens);
        }
    }
    __attribute__((noreturn))
    static void report_error(const Token & token, int start, const char * msg) {
        int col = token.start + 1 + start;
        fprintf(stderr, "%s:%d:%d: error: %s\n", script_path, line_number, col, msg);
        exit_with_error();
    }
    __attribute__((noreturn))
    static void report_error(const Token & token, int start, const char * msg1, const char * msg2) {
        int col = token.start + 1 + start;
        fprintf(stderr, "%s:%d:%d: error: %s%s\n", script_path, line_number, col, msg1, msg2);
        exit_with_error();
    }
    static void expect_extra_token_count(const List<Token> & tokens, int extra_token_count) {
        if (tokens.length() - 1 == extra_token_count)
            return;
        fprintf(stderr, "%s:%d:1: error: expected %d arguments. got %d arguments.\n", script_path, line_number, extra_token_count, tokens.length() - 1);
        exit_with_error();
    }
    static uint32_t parse_nibble(ByteBuffer const& line, const Token & token, int index) {
        uint32_t c = line[token.start + index];
        if ('0' <= c && c <= '9') {
            return  c - '0';
        } else if ('a' <= c && c <= 'f') {
            return c - 'a' + 0xa;
        } else {
            report_error(token, index, "hex digit out of range [0-9a-f]");
        }
    }
    static int parse_int(ByteBuffer const& line, const Token & token) {
        int index = 0;
        int length = token.end - token.start;
        bool negative = false;
        if (line[token.start + index] == '-') {
            negative = true;
            index++;
        }
        if (index == length)
            report_error(token, index, "expected decimal digits");
        int x = 0;
        for (; index < length; index++) {
            uint32_t c = line[token.start + index];
            int digit = c - '0';
            if (digit > 9)
                report_error(token, index, "expected decimal digit");
            // x *= radix;
            if (__builtin_mul_overflow(x, 10, &x))
                report_error(token, index, "integer overflow");

            // x += digit
            if (__builtin_add_overflow(x, digit, &x))
                report_error(token, index, "integer overflow");
        }
        if (negative) {
            // x *= -1;
            if (__builtin_mul_overflow(x, -1, &x))
                report_error(token, 0, "integer overflow");
        }
        return x;
    }
    static uint32_t parse_uint32(ByteBuffer const& line, const Token & token, int start) {
        uint32_t n = 0;
        for (int i = 0; i < 8; i++) {
            uint32_t nibble = parse_nibble(line, token, start + i);
            n |= nibble << ((8 - i - 1) * 4);
        }
        return n;
    }
    static uint32_t parse_uint32(ByteBuffer const& line, const Token & token) {
        if (token.end - token.start != 8)
            report_error(token, 0, "expected hex uint32");
        return parse_uint32(line, token, 0);
    }
    static int64_t parse_int64(ByteBuffer const& line, const Token & token) {
        ByteBuffer buffer;
        buffer.append(line.raw() + token.start, token.end - token.start);
        int64_t value;
        sscanf(buffer.raw(), int64_format, &value);
        return value;
    }
    template <int Size64>
    static uint_oversized<Size64> parse_uint_oversized(ByteBuffer const& line, const Token & token) {
        if (token.end - token.start != 16 * Size64) {
            assert(Size64 == 4); // we'd need different error messages
            report_error(token, 0, "expected hex uint256");
        }
        uint_oversized<Size64> result;
        for (int j = 0; j < Size64; j++) {
            uint64_t n = 0;
            for (int i = 0; i < 16; i++) {
                uint64_t nibble = parse_nibble(line, token, j * 16 + i);
                n |= nibble << ((16 - i - 1) * 4);
            }
            result.values[j] = n;
        }
        return result;
    }
    static inline uint256 parse_uint256(ByteBuffer const& line, const Token & token) {
        return parse_uint_oversized<4>(line, token);
    }

    static Coord parse_coord(ByteBuffer const& line, const Token & token1, const Token & token2) {
        return Coord{parse_int(line, token1), parse_int(line, token2)};
    }

    static String parse_string(ByteBuffer const& line, const Token & token) {
        if (token.end - token.start < 2)
            report_error(token, 0, "expected quoted string");
        if (!(line[token.start] == '"' && line[token.end - 1] == '"'))
            report_error(token, 0, "expected quoted string");
        String result = new_string();
        bool ok;
        result->decode(line, token.start + 1, token.end - 1, &ok);
        if (!ok)
            report_error(token, 0, "invalid UTF-8 string");
        return result;
    }

    static bool token_equals(ByteBuffer const& line, const Token & token, ByteBuffer const& text) {
        if (token.end - token.start != text.length())
            return false;
        return memcmp(line.raw() + token.start, text.raw(), text.length()) == 0;
    }
    static bool token_equals(ByteBuffer const& line, const Token & token, ConstStr text) {
        if (token.end - token.start != text.len)
            return false;
        return memcmp(line.raw() + token.start, text.ptr, text.len) == 0;
    }

    static void expect_token(ByteBuffer const& line, const Token & token, ConstStr expected_text) {
        if (!token_equals(line, token, expected_text))
            report_error(token, 1, "unexpected token");
    }
};

#endif
