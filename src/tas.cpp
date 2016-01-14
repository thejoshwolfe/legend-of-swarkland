#include "tas.hpp"

#include "random.hpp"

#include <stdio.h>
#include <errno.h>

int tas_delay;

static const char * script_path;
static FILE * script_file;
static uint32_t tas_seed;
static int frame_counter;
static TasScriptMode current_mode;

void set_tas_delay(int n) {
    tas_delay = n;
}
uint32_t tas_get_seed() {
    return tas_seed;
}

class Token {
public:
    String string = nullptr;
    int col = 0;
};

static void tokenize_line(const String & line, List<Token> * tokens) {
    int index = 0;
    Token current_token;
    current_token.col = -1;
    for (; index < line->length(); index++) {
        uint32_t c = (*line)[index];
        if (c == '#')
            break;
        if (current_token.col == -1) {
            if (c == ' ')
                continue;
            // start token
            current_token.col = index;
        } else {
            if (c != ' ')
                continue;
            // end token
            current_token.string = line->substring(current_token.col, index);
            tokens->append(current_token);
            current_token = Token();
            current_token.col = -1;
        }
    }
    if (current_token.col != -1) {
        // end token
        current_token.string = line->substring(current_token.col, index);
        tokens->append(current_token);
        current_token = Token();
    }
}

static void write_line(const String & line) {
    ByteBuffer buffer;
    line->encode(&buffer);
    if (fwrite(buffer.raw(), buffer.length(), 1, script_file) < 1) {
        fprintf(stderr, "ERROR: IO error when writing to file: %s\n", script_path);
        exit(1);
    }
    fflush(script_file);
}
ByteBuffer read_buffer;
static String read_line() {
    int index = 0;
    while (true) {
        // look for an EOL
        for (; index < read_buffer.length(); index++) {
            if (read_buffer[index] != '\n')
                continue;
            String line = new_string();
            bool ok;
            line->decode(read_buffer, 0, index, &ok);
            if (!ok) {
                // TODO: line number error
                exit(1);
            }
            read_buffer.remove_range(0, index + 1);
            return line;
        }

        // read more chars
        char blob[256];
        size_t read_count = fread(blob, 1, 256, script_file);
        read_buffer.append(blob, read_count);
    }
}

static String uint32_to_string(uint32_t n) {
    // python: hex(n)[2:].zfill(8)
    String string = new_string();
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = (n >> (32 - (i + 1) * 4)) & 0xf;
        char c = nibble <= 9 ? nibble + '0' : (nibble - 0xa) + 'a';
        string->append(c);
    }
    return string;
}
static uint32_t parse_uint32(const Token & token) {
    if (token.string->length() != 8) {
        // TODO: error with line number
        exit(1);
    }
    uint32_t n = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t c = (*token.string)[i];
        uint32_t nibble;
        if ('0' <= c && c <= '9') {
            nibble = c - '0';
        } else if ('a' <= c && c <= 'f') {
            nibble = c - 'a' + 0xa;
        } else {
            // TODO: error with line number
            exit(1);
        }
        n |= nibble << ((8 - i - 1) * 4);
    }
    return n;
}

static const char * const SEED = "@seed";
static void write_seed(uint32_t seed) {
    String line = new_string();
    line->format("%s %s\n", SEED, uint32_to_string(seed));
    write_line(line);
}
static uint32_t read_seed() {
    List<Token> tokens;
    while (tokens.length() == 0) {
        String line = read_line();
        tokenize_line(line, &tokens);
    }
    if (tokens.length() != 2) {
        // TODO: error with line number
        exit(1);
    }
    if (*tokens[0].string != *new_string(SEED)) {
        // TODO: error with line number
        exit(1);
    }
    return parse_uint32(tokens[1]);
}

void set_tas_script(TasScriptMode mode, const char * file_path) {
    script_path = file_path;

    switch (mode) {
        case TasScriptMode_WRITE:
            script_file = fopen(script_path, "wb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                exit(1);
            }
            current_mode = TasScriptMode_WRITE;
            break;
        case TasScriptMode_READ:
            script_file = fopen(script_path, "rb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not read file: %s\n", script_path);
                exit(1);
            }
            current_mode = TasScriptMode_READ;
            break;
        case TasScriptMode_READ_WRITE:
            script_file = fopen(script_path, "r+b");
            if (script_file != nullptr) {
                // first read, then write
                current_mode = TasScriptMode_READ_WRITE;
            } else {
                if (errno != ENOENT) {
                    fprintf(stderr, "ERROR: could not read/create file: %s\n", script_path);
                    exit(1);
                }
                // no problem. we'll just make it
                script_file = fopen(script_path, "wb");
                if (script_file == nullptr) {
                    fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                    exit(1);
                }
                current_mode = TasScriptMode_WRITE;
            }
            break;
        case TasScriptMode_IGNORE:
            current_mode = TasScriptMode_IGNORE;
            break;
    }

    switch (current_mode) {
        case TasScriptMode_READ:
        case TasScriptMode_READ_WRITE: {
            tas_seed = read_seed();
            break;
        }
        case TasScriptMode_WRITE: {
            tas_seed = get_random_seed();
            write_seed(tas_seed);
            break;
        }
        case TasScriptMode_IGNORE:
            tas_seed = get_random_seed();
            break;
    }
}

static Action read_action() {
    Action result;
    if (fread(&result, sizeof(Action), 1, script_file) < 1) {
        // end of script
        return Action::undecided();
    }
    if (result == Action::undecided())
        panic("read indecision from input script");
    return result;
}

Action tas_get_decision() {
    if (tas_delay > 0) {
        if (frame_counter < tas_delay) {
            frame_counter++;
            return Action::undecided(); // let the screen draw
        }
        frame_counter = 0;
    }
    switch (current_mode) {
        case TasScriptMode_READ_WRITE: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                current_mode = TasScriptMode_WRITE;
            }
            return result;
        }
        case TasScriptMode_READ: {
            Action result = read_action();
            if (result == Action::undecided()) {
                // end of file
                fclose(script_file);
                current_mode = TasScriptMode_IGNORE;
            }
            return result;
        }
        case TasScriptMode_WRITE:
        case TasScriptMode_IGNORE:
            // no, you decide.
            return Action::undecided();
    }
    unreachable();
}

void tas_record_decision(Action action) {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't write
            break;
        case TasScriptMode_WRITE:
            fwrite(&action, sizeof(Action), 1, script_file);
            fflush(script_file);
            break;
    }
}

void tas_delete_save() {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't delete it
            break;
        case TasScriptMode_WRITE:
            fclose(script_file);
            remove(script_path);
            current_mode = TasScriptMode_IGNORE;
            break;
    }
}
