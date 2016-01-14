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
        if (read_count == 0) {
            return nullptr;
        }
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
template<int Size64>
static String uint_oversized_to_string(uint_oversized<Size64> n) {
    String string = new_string();
    for (int j = 0; j < Size64; j++) {
        for (int i = 0; i < 16; i++) {
            uint32_t nibble = (n.values[j] >> (64 - (i + 1) * 4)) & 0xf;
            char c = nibble <= 9 ? nibble + '0' : (nibble - 0xa) + 'a';
            string->append(c);
        }
    }
    return string;
}
static String uint256_to_string(uint256 n) {
    return uint_oversized_to_string(n);
}
static String int_to_string(int n) {
    String result = new_string();
    if (n == -0x7fffffff) {
        // special case, because this value is unnegatable.
        result->append("-2147483647");
        return result;
    }
    if (n < 0) {
        result->append('-');
        n = -n;
    }
    bool print_now = false;
    for (int place_value = 1000000000; place_value > 0; place_value /= 10) {
        int digit = n / place_value;
        if (print_now || digit != 0) {
            result->append('0' + digit);
            n -= digit * place_value;
            print_now = true;
        }
    }
    if (!print_now) {
        result->append('0');
    }
    return result;
}

static uint32_t parse_nibble(uint32_t c) {
    if ('0' <= c && c <= '9') {
        return  c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 0xa;
    } else {
        // TODO: error with line number
        exit(1);
    }
}
static int parse_int(const Token & token) {
    int index = 0;
    bool negative = false;
    if (token.string->length() >= 1 && (*token.string)[index] == '-') {
        negative = true;
        index++;
    }
    if (token.string->length() == index) {
        // TODO: error with line number
        exit(1);
    }
    int x = 0;
    for (; index < token.string->length(); index++) {
        uint32_t c = (*token.string)[index];
        int digit = c - '0';
        if (digit > 9) {
            // TODO: error with line number
            exit(1);
        }
        // x *= radix;
        if (__builtin_mul_overflow(x, 10, &x)) {
            // TODO: error with line number
            exit(1);
        }

        // x += digit
        if (__builtin_add_overflow(x, digit, &x)) {
            // TODO: error with line number
            exit(1);
        }
    }
    if (negative) {
        // x *= -1;
        if (__builtin_mul_overflow(x, -1, &x)) {
            // TODO: error with line number
            exit(1);
        }
    }
    return x;
}
static uint32_t parse_uint32(const Token & token) {
    if (token.string->length() != 8) {
        // TODO: error with line number
        exit(1);
    }
    uint32_t n = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t c = (*token.string)[i];
        uint32_t nibble = parse_nibble(c);
        n |= nibble << ((8 - i - 1) * 4);
    }
    return n;
}
template <int Size64>
static uint_oversized<Size64> parse_uint_oversized(const Token & token) {
    if (token.string->length() != 16 * Size64) {
        // TODO: error with line number
        exit(1);
    }
    uint_oversized<Size64> result;
    for (int j = 0; j < Size64; j++) {
        uint64_t n = 0;
        for (int i = 0; i < 16; i++) {
            uint32_t c = (*token.string)[j * 16 + i];
            uint64_t nibble = parse_nibble(c);
            n |= nibble << ((16 - i - 1) * 4);
        }
        result.values[j] = n;
    }
    return result;
}
static inline uint256 parse_uint256(const Token & token) {
    return parse_uint_oversized<4>(token);
}

static Coord parse_coord(const Token & token1, const Token & token2) {
    return Coord{parse_int(token1), parse_int(token2)};
}

// TODO: this should be _COUNT
static String action_type_names[Action::Type::COUNT];
static void init_action_type_names() {
    action_type_names[Action::Type::MOVE] = new_string("move");
    action_type_names[Action::Type::WAIT] = new_string("wait");
    action_type_names[Action::Type::ATTACK] = new_string("attack");
    action_type_names[Action::Type::ZAP] = new_string("zap");
    action_type_names[Action::Type::PICKUP] = new_string("pickup");
    action_type_names[Action::Type::DROP] = new_string("drop");
    action_type_names[Action::Type::QUAFF] = new_string("quaff");
    action_type_names[Action::Type::THROW] = new_string("throw");
    action_type_names[Action::Type::GO_DOWN] = new_string("down");

    action_type_names[Action::Type::CHEATCODE_HEALTH_BOOST] = new_string("!health");
    action_type_names[Action::Type::CHEATCODE_KILL_EVERYBODY_IN_THE_WORLD] = new_string("!kill");
    action_type_names[Action::Type::CHEATCODE_POLYMORPH] = new_string("!polymorph");
    action_type_names[Action::Type::CHEATCODE_GENERATE_MONSTER] = new_string("!monster");
    action_type_names[Action::Type::CHEATCODE_CREATE_ITEM] = new_string("!items");
    action_type_names[Action::Type::CHEATCODE_IDENTIFY] = new_string("!identify");
    action_type_names[Action::Type::CHEATCODE_GO_DOWN] = new_string("!down");
    action_type_names[Action::Type::CHEATCODE_GAIN_LEVEL] = new_string("!levelup");
}

static Action::Type parse_action_type(const Token & token) {
    for (int i = 0; i < Action::Type::COUNT; i++) {
        if (*action_type_names[i] == *token.string)
            return (Action::Type)i;
    }
    // TODO: error with line number
    exit(1);
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
        if (line == nullptr) {
            // TODO: error with line number
            exit(1);
        }
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
    init_action_type_names();
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
    List<Token> tokens;
    while (tokens.length() == 0) {
        String line = read_line();
        if (line == nullptr)
            return Action::undecided(); // EOF
        tokenize_line(line, &tokens);
    }
    if (tokens.length() != 4) {
        // TODO: error line number
        exit(1);
    }
    Action::Type action_type = parse_action_type(tokens[0]);
    uint256 id = parse_uint256(tokens[1]);
    Coord coord = parse_coord(tokens[2], tokens[3]);
    return Action{action_type, id, coord};
}
static String action_to_string(const Action & action) {
    assert(action.type < Action::Type::COUNT);
    String action_type_string = action_type_names[action.type];
    String id_string = uint256_to_string(action.item);
    String coord_string1 = int_to_string(action.coord.x);
    String coord_string2 = int_to_string(action.coord.y);
    String result = new_string();
    result->format("%s %s %s %s\n", action_type_string, id_string, coord_string1, coord_string2);
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

void tas_record_decision(const Action & action) {
    switch (current_mode) {
        case TasScriptMode_READ_WRITE:
        case TasScriptMode_READ:
        case TasScriptMode_IGNORE:
            // don't write
            break;
        case TasScriptMode_WRITE:
            write_line(action_to_string(action));
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
