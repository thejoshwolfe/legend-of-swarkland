#include "random.hpp"

#include "display.hpp"

#include <stdio.h>
#include <errno.h>
#include <serial.hpp>
#include <inttypes.h>

static int replay_delay;

static const char * script_path;
static FILE * script_file;
static uint32_t rng_seed;
static bool lets_do_test_mode;
static int replay_delay_frame_counter;
static SaveFileMode current_mode;

// line_number starts at 1
static int line_number = 0;

__attribute__((noreturn))
static void exit_with_error() {
    // put a breakpoint here
    exit(1);
}
__attribute__((noreturn))
static void test_expect_fail(const char * message) {
    fprintf(stderr, "%s:%d:1: error: %s\n", script_path, line_number, message);
    exit_with_error();
}
__attribute__((noreturn))
static void test_expect_fail(const char * fmt, String s) {
    ByteBuffer buffer;
    s->encode(&buffer);
    ByteBuffer msg;
    msg.format(fmt, buffer.raw());
    fprintf(stderr, "%s:%d:1: error: %s\n", script_path, line_number, msg.raw());
    exit_with_error();
}
__attribute__((noreturn))
static void test_expect_fail(const char * fmt, String s1, String s2) {
    ByteBuffer buffer1;
    s1->encode(&buffer1);
    ByteBuffer buffer2;
    s2->encode(&buffer2);
    ByteBuffer msg;
    msg.format(fmt, buffer1.raw(), buffer2.raw());
    fprintf(stderr, "%s:%d:1: error: %s\n", script_path, line_number, msg.raw());
    exit_with_error();
}

void set_replay_delay(int n) {
    replay_delay = n;
}
void init_random() {
    if (lets_do_test_mode)
        game->test_mode = true;
    else
        init_random_state(&game->the_random_state, rng_seed);
}

static void write_buffer(const ByteBuffer & buffer) {
    if (fwrite(buffer.raw(), buffer.length(), 1, script_file) < 1) {
        fprintf(stderr, "ERROR: IO error when writing to file: %s\n", script_path);
        exit_with_error();
    }
    fflush(script_file);
}
static void write_line(const String & line) {
    ByteBuffer buffer;
    line->encode(&buffer);
    write_buffer(buffer);
}
ByteBuffer read_buffer;
static String read_line() {
    int index = 0;
    while (true) {
        // look for an EOL
        for (; index < read_buffer.length(); index++) {
            if (read_buffer[index] != '\n')
                continue;
            line_number++;
            String line = new_string();
            bool ok;
            line->decode(read_buffer, 0, index, &ok);
            if (!ok) {
                fprintf(stderr, "ERROR: unable to decode file as UTF-8: %s\n", script_path);
                exit_with_error();
            }
            read_buffer.remove_range(0, index + 1);
            return line;
        }

        // read more chars
        char blob[256];
        size_t read_count = fread(blob, 1, 256, script_file);
        if (read_count == 0) {
            if (read_buffer.length() > 0) {
                fprintf(stderr, "ERROR: expected newline at end of file: %s\n", script_path);
                exit_with_error();
            }
            return nullptr;
        }
        read_buffer.append(blob, read_count);
    }
}

class Token {
public:
    String string = nullptr;
    // col starts at 1
    int col = 0;
};
static void tokenize_line(const String & line, List<Token> * tokens) {
    int index = 0;
    Token current_token;
    for (; index < line->length(); index++) {
        uint32_t c = (*line)[index];
        if (c == '#')
            break;
        if (current_token.col == 0) {
            if (c == ' ')
                continue;
            // start token
            current_token.col = index + 1;
            if (c == '"') {
                // start quoted string
                while (true) {
                    index++;
                    if (index >= line->length()) {
                        fprintf(stderr, "%s:%d: error: unterminated quoted string\n", script_path, line_number);
                        exit_with_error();
                    }
                    c = (*line)[index];
                    if (c == '"') {
                        // end quoted string
                        current_token.string = line->substring(current_token.col - 1, index + 1);
                        tokens->append(current_token);
                        current_token = Token();
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
            current_token.string = line->substring(current_token.col - 1, index);
            tokens->append(current_token);
            current_token = Token();
        }
    }
    if (current_token.col != 0) {
        // end token
        current_token.string = line->substring(current_token.col - 1, index);
        tokens->append(current_token);
    }
}
static void read_tokens(List<Token> * tokens, bool tolerate_eof) {
    while (tokens->length() == 0) {
        String line = read_line();
        if (line == nullptr) {
            // EOF
            if (!tolerate_eof) {
                fprintf(stderr, "%s:%d:1: error: unexpected EOF", script_path, line_number);
                exit_with_error();
            }
            return;
        }
        tokenize_line(line, tokens);
    }
}
__attribute__((noreturn))
static void report_error(const Token & token, int start, const char * msg) {
    int col = token.col + start;
    fprintf(stderr, "%s:%d:%d: error: %s\n", script_path, line_number, col, msg);
    exit_with_error();
}
__attribute__((noreturn))
static void report_error(const Token & token, int start, const char * msg1, const char * msg2) {
    int col = token.col + start;
    fprintf(stderr, "%s:%d:%d: error: %s%s\n", script_path, line_number, col, msg1, msg2);
    exit_with_error();
}
static void expect_extra_token_count(const List<Token> & tokens, int extra_token_count) {
    if (tokens.length() - 1 == extra_token_count)
        return;
    fprintf(stderr, "%s:%d:1: error: expected %d arguments. got %d arguments.\n", script_path, line_number, extra_token_count, tokens.length() - 1);
    exit_with_error();
}

static char nibble_to_char(uint32_t nibble) {
    assert(nibble < 0x10);
    return "0123456789abcdef"[nibble];
}
static String uint32_to_string(uint32_t n) {
    // python: hex(n)[2:].zfill(8)
    String string = new_string();
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = (n >> (32 - (i + 1) * 4)) & 0xf;
        char c = nibble_to_char(nibble);
        string->append(c);
    }
    return string;
}
template<int Size64>
static void write_uint_oversized_to_buffer(uint_oversized<Size64> n, ByteBuffer * output_buffer) {
    for (int j = 0; j < Size64; j++) {
        for (int i = 0; i < 16; i++) {
            uint32_t nibble = (n.values[j] >> (64 - (i + 1) * 4)) & 0xf;
            char c = nibble_to_char(nibble);
            output_buffer->append(c);
        }
    }
}
static String uint256_to_string(uint256 n) {
    ByteBuffer buffer;
    write_uint_oversized_to_buffer(n, &buffer);
    return new_string(buffer);
}
static String int_to_string(int n) {
    String result = new_string();
    if (n == -2147483648) {
        // special case, because this value is unnegatable.
        result->append("-2147483648");
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

static uint32_t parse_nibble(const Token & token, int index) {
    uint32_t c = (*token.string)[index];
    if ('0' <= c && c <= '9') {
        return  c - '0';
    } else if ('a' <= c && c <= 'f') {
        return c - 'a' + 0xa;
    } else {
        report_error(token, index, "hex digit out of range [0-9a-f]");
    }
}
static int parse_int(const Token & token) {
    int index = 0;
    bool negative = false;
    if (token.string->length() >= 1 && (*token.string)[index] == '-') {
        negative = true;
        index++;
    }
    if (token.string->length() == index)
        report_error(token, index, "expected decimal digits");
    int x = 0;
    for (; index < token.string->length(); index++) {
        uint32_t c = (*token.string)[index];
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
static uint32_t parse_uint32(const Token & token) {
    if (token.string->length() != 8)
        report_error(token, 0, "expected hex uint32");
    uint32_t n = 0;
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = parse_nibble(token, i);
        n |= nibble << ((8 - i - 1) * 4);
    }
    return n;
}
static int64_t parse_int64(const Token & token) {
    ByteBuffer buffer;
    token.string->encode(&buffer);
    int64_t value;
    sscanf(buffer.raw(), "%" PRIi64, &value);
    return value;
}
template <int Size64>
static uint_oversized<Size64> parse_uint_oversized(const Token & token) {
    if (token.string->length() != 16 * Size64) {
        assert(Size64 == 4); // we'd need different error messages
        report_error(token, 0, "expected hex uint256");
    }
    uint_oversized<Size64> result;
    for (int j = 0; j < Size64; j++) {
        uint64_t n = 0;
        for (int i = 0; i < 16; i++) {
            uint64_t nibble = parse_nibble(token, j * 16 + i);
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

static String parse_string(const Token & token) {
    if (token.string->length() < 2)
        report_error(token, 0, "expected quoted string");
    if (!((*token.string)[0] == '"' && (*token.string)[token.string->length() - 1] == '"'))
        report_error(token, 0, "expected quoted string");
    return token.string->substring(1, token.string->length() - 1);
}
static void expect_token(const Token & token, const char * expected_text) {
    if (*token.string != *new_string(expected_text))
        report_error(token, 1, "unexpected token");
}

enum DirectiveId {
    DirectiveId_MARK_EVENTS,
    DirectiveId_EXPECT_EVENT,
    DirectiveId_EXPECT_NO_EVENTS,
    DirectiveId_FIND_THINGS_AT,
    DirectiveId_EXPECT_THING,
    DirectiveId_EXPECT_NOTHING,
    DirectiveId_EXPECT_CARRYING,
    DirectiveId_EXPECT_CARRYING_NOTHING,
    DirectiveId_SNAPSHOT,
    DirectiveId_MAP_TILES,
    DirectiveId_AESTHETIC_INDEXES,
    DirectiveId_RNG_STATE,
    DirectiveId_THING,
    DirectiveId_LIFE,
    DirectiveId_STATUS_EFFECT,
    DirectiveId_ABILITY_COOLDOWN,
    DirectiveId_KNOWLEDGE,
    DirectiveId_PERCEIVED_THING,

    DirectiveId_COUNT,
};

static String action_names[Action::COUNT];
static String directive_names[DirectiveId_COUNT];
static String species_names[SpeciesId_COUNT];
static String decision_maker_names[DecisionMakerType_COUNT];
static String thing_type_names[ThingType_COUNT];
static String wand_id_names[WandId_COUNT];
static String potion_id_names[PotionId_COUNT];
static String book_id_names[BookId_COUNT];
static String ability_names[AbilityId_COUNT];
static IndexAndValue<char> constexpr tile_type_short_names[TileType_COUNT] = {
    {TileType_UNKNOWN, '_'},
    {TileType_DIRT_FLOOR, 'a'},
    {TileType_MARBLE_FLOOR, 'b'},
    {TileType_BROWN_BRICK_WALL, 'c'},
    {TileType_GRAY_BRICK_WALL, 'd'},
    {TileType_BORDER_WALL, 'e'},
    {TileType_STAIRS_DOWN, 'f'},
    {TileType_UNKNOWN_FLOOR, '1'},
    {TileType_UNKNOWN_WALL, '2'},
}; static_assert(_check_indexed_array(tile_type_short_names, TileType_COUNT), "missed a spot");
static IndexAndValue<char const*> constexpr status_effect_names[StatusEffect::COUNT] = {
    {StatusEffect::CONFUSION, "confusion"},
    {StatusEffect::SPEED, "speed"},
    {StatusEffect::ETHEREAL_VISION, "ethereal_vision"},
    {StatusEffect::COGNISCOPY, "cogniscopy"},
    {StatusEffect::BLINDNESS, "blindness"},
    {StatusEffect::INVISIBILITY, "invisibility"},
    {StatusEffect::POISON, "poison"},
    {StatusEffect::POLYMORPH, "polymorph"},
    {StatusEffect::SLOWING, "slowing"},
}; static_assert(_check_indexed_array(status_effect_names, StatusEffect::COUNT), "missed a spot");
static IndexAndValue<char const*> constexpr wand_description_names[WandDescriptionId_COUNT] = {
    {WandDescriptionId_BONE_WAND, "bone"},
    {WandDescriptionId_GOLD_WAND, "gold"},
    {WandDescriptionId_PLASTIC_WAND, "plastic"},
    {WandDescriptionId_COPPER_WAND, "copper"},
    {WandDescriptionId_PURPLE_WAND, "purple"},
    {WandDescriptionId_SHINY_BONE_WAND, "shiny_bone"},
    {WandDescriptionId_SHINY_GOLD_WAND, "shiny_gold"},
    {WandDescriptionId_SHINY_PLASTIC_WAND, "shiny_plastic"},
    {WandDescriptionId_SHINY_COPPER_WAND, "shiny_copper"},
    {WandDescriptionId_SHINY_PURPLE_WAND, "shiny_purple"},
}; static_assert(_check_indexed_array(wand_description_names, WandDescriptionId_COUNT), "missed a spot");
static IndexAndValue<char const*> constexpr potion_description_names[PotionDescriptionId_COUNT] = {
    {PotionDescriptionId_BLUE_POTION, "blue"},
    {PotionDescriptionId_GREEN_POTION, "green"},
    {PotionDescriptionId_RED_POTION, "red"},
    {PotionDescriptionId_YELLOW_POTION, "yellow"},
    {PotionDescriptionId_ORANGE_POTION, "orange"},
    {PotionDescriptionId_PURPLE_POTION, "purple"},
}; static_assert(_check_indexed_array(potion_description_names, PotionDescriptionId_COUNT), "missed a spot");
static IndexAndValue<char const*> constexpr book_description_names[BookDescriptionId_COUNT] = {
    {BookDescriptionId_PURPLE_BOOK, "purple"},
    {BookDescriptionId_BLUE_BOOK, "blue"},
    {BookDescriptionId_RED_BOOK, "red"},
    {BookDescriptionId_GREEN_BOOK, "green"},
    {BookDescriptionId_YELLOW_BOOK, "yellow"},
}; static_assert(_check_indexed_array(book_description_names, BookDescriptionId_COUNT), "missed a spot");

static char const* const RNG_DIRECTIVE = "@rng";
static char const* const SEED_DIRECTIVE = "@seed";
static char const* const TEST_MODE_HEADER = "@test";
static char const* const SNAPSHOT_DIRECTIVE = "@snapshot";
static char const* const MAP_TILES_DIRECTIVE = "@map_tiles";
static char const* const AESTHETIC_INDEXES_DIRECTIVE = "@aesthetics";
static char const* const RNG_STATE_DIRECTIVE = "@rng_state";
static char const* const THING_DIRECTIVE = "@thing";
static char const* const LIFE_DIRECTIVE = "@life";
static char const* const STATUS_EFFECT_DIRECTIVE = "@status_effect";
static char const* const ABILITY_COOLDOWN_DIRECTIVE = "@ability_cooldown";
static char const* const KNOWLEDGE_DIRECTIVE = "@knowledge";
static char const* const PERCEIVED_THING_DIRECTIVE = "@perceived_thing";

// making this a macro makes the red squiggly from the panic() show up at the call site instead of here.
#define check_no_nulls(array) \
    for (int i = 0; i < (int)(sizeof(array) / sizeof(array[0])); i++) \
        assert_str(array[i] != nullptr, "missed a spot")

static void init_name_arrays() {
    action_names[Action::MOVE] = new_string("move");
    action_names[Action::WAIT] = new_string("wait");
    action_names[Action::ATTACK] = new_string("attack");
    action_names[Action::ZAP] = new_string("zap");
    action_names[Action::PICKUP] = new_string("pickup");
    action_names[Action::DROP] = new_string("drop");
    action_names[Action::QUAFF] = new_string("quaff");
    action_names[Action::READ_BOOK] = new_string("read_book");
    action_names[Action::THROW] = new_string("throw");
    action_names[Action::GO_DOWN] = new_string("down");
    action_names[Action::ABILITY] = new_string("ability");
    action_names[Action::CHEATCODE_HEALTH_BOOST] = new_string("!health");
    action_names[Action::CHEATCODE_KILL] = new_string("!kill");
    action_names[Action::CHEATCODE_POLYMORPH] = new_string("!polymorph");
    action_names[Action::CHEATCODE_GENERATE_MONSTER] = new_string("!monster");
    action_names[Action::CHEATCODE_WISH] = new_string("!wish");
    action_names[Action::CHEATCODE_IDENTIFY] = new_string("!identify");
    action_names[Action::CHEATCODE_GO_DOWN] = new_string("!down");
    action_names[Action::CHEATCODE_GAIN_LEVEL] = new_string("!levelup");
    check_no_nulls(action_names);

    directive_names[DirectiveId_MARK_EVENTS] = new_string("@mark_events");
    directive_names[DirectiveId_EXPECT_EVENT] = new_string("@expect_event");
    directive_names[DirectiveId_EXPECT_NO_EVENTS] = new_string("@expect_no_events");
    directive_names[DirectiveId_FIND_THINGS_AT] = new_string("@find_things_at");
    directive_names[DirectiveId_EXPECT_THING] = new_string("@expect_thing");
    directive_names[DirectiveId_EXPECT_NOTHING] = new_string("@expect_nothing");
    directive_names[DirectiveId_EXPECT_CARRYING] = new_string("@expect_carrying");
    directive_names[DirectiveId_EXPECT_CARRYING_NOTHING] = new_string("@expect_carrying_nothing");
    directive_names[DirectiveId_SNAPSHOT] = new_string(SNAPSHOT_DIRECTIVE);
    directive_names[DirectiveId_MAP_TILES] = new_string(MAP_TILES_DIRECTIVE);
    directive_names[DirectiveId_AESTHETIC_INDEXES] = new_string(AESTHETIC_INDEXES_DIRECTIVE);
    directive_names[DirectiveId_RNG_STATE] = new_string(RNG_STATE_DIRECTIVE);
    directive_names[DirectiveId_THING] = new_string(THING_DIRECTIVE);
    directive_names[DirectiveId_LIFE] = new_string(LIFE_DIRECTIVE);
    directive_names[DirectiveId_STATUS_EFFECT] = new_string(STATUS_EFFECT_DIRECTIVE);
    directive_names[DirectiveId_ABILITY_COOLDOWN] = new_string(ABILITY_COOLDOWN_DIRECTIVE);
    directive_names[DirectiveId_KNOWLEDGE] = new_string(KNOWLEDGE_DIRECTIVE);
    directive_names[DirectiveId_PERCEIVED_THING] = new_string(PERCEIVED_THING_DIRECTIVE);
    check_no_nulls(directive_names);

    species_names[SpeciesId_HUMAN] = new_string("human");
    species_names[SpeciesId_OGRE] = new_string("ogre");
    species_names[SpeciesId_LICH] = new_string("lich");
    species_names[SpeciesId_PINK_BLOB] = new_string("pink_blob");
    species_names[SpeciesId_AIR_ELEMENTAL] = new_string("air_elemenetal");
    species_names[SpeciesId_DOG] = new_string("dog");
    species_names[SpeciesId_ANT] = new_string("ant");
    species_names[SpeciesId_BEE] = new_string("bee");
    species_names[SpeciesId_BEETLE] = new_string("beetle");
    species_names[SpeciesId_SCORPION] = new_string("scorpion");
    species_names[SpeciesId_SNAKE] = new_string("snake");
    species_names[SpeciesId_COBRA] = new_string("cobra");
    check_no_nulls(species_names);

    decision_maker_names[DecisionMakerType_PLAYER] = new_string("player");
    decision_maker_names[DecisionMakerType_AI] = new_string("ai");
    check_no_nulls(decision_maker_names);

    thing_type_names[ThingType_INDIVIDUAL] = new_string("individual");
    thing_type_names[ThingType_WAND] = new_string("wand");
    thing_type_names[ThingType_POTION] = new_string("potion");
    thing_type_names[ThingType_BOOK] = new_string("book");
    check_no_nulls(thing_type_names);

    wand_id_names[WandId_WAND_OF_CONFUSION] = new_string("confusion");
    wand_id_names[WandId_WAND_OF_DIGGING] = new_string("digging");
    wand_id_names[WandId_WAND_OF_MAGIC_MISSILE] = new_string("magic_missile");
    wand_id_names[WandId_WAND_OF_SPEED] = new_string("speed");
    wand_id_names[WandId_WAND_OF_REMEDY] = new_string("remedy");
    wand_id_names[WandId_WAND_OF_BLINDING] = new_string("blinding");
    wand_id_names[WandId_WAND_OF_FORCE] = new_string("force");
    wand_id_names[WandId_WAND_OF_INVISIBILITY] = new_string("invisibility");
    wand_id_names[WandId_WAND_OF_MAGIC_BULLET] = new_string("magic_bullet");
    wand_id_names[WandId_WAND_OF_SLOWING] = new_string("slowing");
    check_no_nulls(wand_id_names);

    potion_id_names[PotionId_POTION_OF_HEALING] = new_string("healing");
    potion_id_names[PotionId_POTION_OF_POISON] = new_string("poison");
    potion_id_names[PotionId_POTION_OF_ETHEREAL_VISION] = new_string("ethereal_vision");
    potion_id_names[PotionId_POTION_OF_COGNISCOPY] = new_string("cogniscopy");
    potion_id_names[PotionId_POTION_OF_BLINDNESS] = new_string("blindness");
    potion_id_names[PotionId_POTION_OF_INVISIBILITY] = new_string("invisibility");
    check_no_nulls(potion_id_names);

    book_id_names[BookId_SPELLBOOK_OF_MAGIC_BULLET] = new_string("magic_bullet");
    book_id_names[BookId_SPELLBOOK_OF_SPEED] = new_string("speed");
    book_id_names[BookId_SPELLBOOK_OF_MAPPING] = new_string("mapping");
    book_id_names[BookId_SPELLBOOK_OF_FORCE] = new_string("force");
    book_id_names[BookId_SPELLBOOK_OF_ASSUME_FORM] = new_string("assume_form");
    check_no_nulls(book_id_names);

    ability_names[AbilityId_SPIT_BLINDING_VENOM] = new_string("spit_blinding_venom");
    check_no_nulls(ability_names);
}

static DirectiveId parse_directive_id(const String & string) {
    for (int i = 0; i < DirectiveId_COUNT; i++) {
        if (*directive_names[i] == *string)
            return (DirectiveId)i;
    }
    // nope
    return DirectiveId_COUNT;
}
static Action::Id parse_action_id(const Token & token) {
    for (int i = 0; i < Action::COUNT; i++) {
        if (*action_names[i] == *token.string)
            return (Action::Id)i;
    }
    if (*token.string == *new_string(RNG_DIRECTIVE))
        report_error(token, 0, "unexpected rng directive");
    report_error(token, 0, "undefined action name");
}
static SpeciesId parse_species_id(const Token & token) {
    for (int i = 0; i < SpeciesId_COUNT; i++) {
        if (*species_names[i] == *token.string)
            return (SpeciesId)i;
    }
    if (*token.string == *new_string("unseen"))
        return SpeciesId_UNSEEN;
    report_error(token, 0, "undefined species id");
}
static DecisionMakerType parse_decision_maker(const Token & token) {
    for (int i = 0; i < DecisionMakerType_COUNT; i++) {
        if (*decision_maker_names[i] == *token.string)
            return (DecisionMakerType)i;
    }
    report_error(token, 0, "undefined decision maker");
}
static ThingType parse_thing_type(const Token & token) {
    for (int i = 0; i < ThingType_COUNT; i++) {
        if (*thing_type_names[i] == *token.string)
            return (ThingType)i;
    }
    report_error(token, 0, "undefined thing type");
}
static WandId parse_wand_id(const Token & token) {
    for (int i = 0; i < WandId_COUNT; i++) {
        if (*wand_id_names[i] == *token.string)
            return (WandId)i;
    }
    if (*token.string == *new_string("unknown"))
        return WandId_UNKNOWN;
    report_error(token, 0, "undefined wand id");
}
static PotionId parse_potion_id(const Token & token) {
    for (int i = 0; i < PotionId_COUNT; i++) {
        if (*potion_id_names[i] == *token.string)
            return (PotionId)i;
    }
    if (*token.string == *new_string("unknown"))
        return PotionId_UNKNOWN;
    report_error(token, 0, "undefined potion id");
}
static BookId parse_book_id(const Token & token) {
    for (int i = 0; i < BookId_COUNT; i++) {
        if (*book_id_names[i] == *token.string)
            return (BookId)i;
    }
    if (*token.string == *new_string("unknown"))
        return BookId_UNKNOWN;
    report_error(token, 0, "undefined book id");
}
static Action::Thing parse_thing_signature(const Token & token1, const Token & token2) {
    Action::Thing thing;
    thing.thing_type = parse_thing_type(token1);
    switch (thing.thing_type) {
        case ThingType_INDIVIDUAL:
            thing.species_id = parse_species_id(token2);
            break;
        case ThingType_WAND:
            thing.wand_id = parse_wand_id(token2);
            break;
        case ThingType_POTION:
            thing.potion_id = parse_potion_id(token2);
            break;
        case ThingType_BOOK:
            thing.book_id = parse_book_id(token2);
            break;

        case ThingType_COUNT:
            unreachable();
    }
    return thing;
}
static AbilityId parse_ability_id(const Token & token) {
    for (int i = 0; i < AbilityId_COUNT; i++) {
        if (*ability_names[i] == *token.string)
            return (AbilityId)i;
    }
    report_error(token, 0, "undefined ability id");
}
static TileType parse_tile_type_short_name(const Token & token, char c) {
    for (int i = 0; i < TileType_COUNT; i++) {
        if (tile_type_short_names[i].value == c)
            return (TileType)i;
    }
    report_error(token, 0, "undefined tile type");
}

static String rng_input_to_string(const ByteBuffer & tag, int value) {
    String line = new_string();
    line->format("  %s %d ", RNG_DIRECTIVE, value);
    line->decode(tag);
    line->append("\n");
    return line;
}
static int read_rng_input(const ByteBuffer & tag) {
    List<Token> tokens;
    read_tokens(&tokens, false);
    if (*tokens[0].string != *new_string(RNG_DIRECTIVE))
        report_error(tokens[0], 0, "expected rng directive with tag: ", tag.raw());
    if (tokens.length() != 3)
        report_error(tokens[0], 0, "expected 2 arguments");
    if (*tokens[2].string != *new_string(tag))
        report_error(tokens[2], 0, "rng tag mismatch. expected: ", tag.raw());
    return parse_int(tokens[1]);
}

static void write_seed(uint32_t seed) {
    String line = new_string();
    line->format("%s %s\n", SEED_DIRECTIVE, uint32_to_string(seed));
    write_line(line);
}
static void write_test_mode_header() {
    String line = new_string();
    line->format("%s\n", TEST_MODE_HEADER);
    write_line(line);
}
static void read_header() {
    List<Token> tokens;
    read_tokens(&tokens, false);
    if (*tokens[0].string == *new_string(SEED_DIRECTIVE)) {
        if (tokens.length() != 2) {
            fprintf(stderr, "%s:%d:1: error: expected 1 argument", script_path, line_number);
            exit_with_error();
        }
        rng_seed = parse_uint32(tokens[1]);
    } else if (*tokens[0].string == *new_string(TEST_MODE_HEADER)) {
        if (tokens.length() != 1) {
            fprintf(stderr, "%s:%d:1: error: expected no arguments", script_path, line_number);
            exit_with_error();
        }
        lets_do_test_mode = true;
    } else {
        fprintf(stderr, "%s:%d:1: error: expected swarkland header", script_path, line_number);
        exit_with_error();
    }
}

void set_save_file(SaveFileMode mode, const char * file_path, bool cli_syas_test_mode) {
    init_name_arrays();
    script_path = file_path;

    switch (mode) {
        case SaveFileMode_WRITE:
            script_file = fopen(script_path, "wb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                exit_with_error();
            }
            current_mode = SaveFileMode_WRITE;
            break;
        case SaveFileMode_READ:
            script_file = fopen(script_path, "rb");
            if (script_file == nullptr) {
                fprintf(stderr, "ERROR: could not read file: %s\n", script_path);
                exit_with_error();
            }
            current_mode = SaveFileMode_READ;
            break;
        case SaveFileMode_READ_WRITE:
            script_file = fopen(script_path, "r+b");
            if (script_file != nullptr) {
                // first read, then write
                current_mode = SaveFileMode_READ_WRITE;
            } else {
                if (errno != ENOENT) {
                    fprintf(stderr, "ERROR: could not read/create file: %s\n", script_path);
                    exit_with_error();
                }
                // no problem. we'll just make it
                script_file = fopen(script_path, "wb");
                if (script_file == nullptr) {
                    fprintf(stderr, "ERROR: could not create file: %s\n", script_path);
                    exit_with_error();
                }
                current_mode = SaveFileMode_WRITE;
            }
            break;
        case SaveFileMode_IGNORE:
            current_mode = SaveFileMode_IGNORE;
            break;
    }

    switch (current_mode) {
        case SaveFileMode_READ:
        case SaveFileMode_READ_WRITE: {
            read_header();
            break;
        }
        case SaveFileMode_WRITE: {
            if (cli_syas_test_mode) {
                write_test_mode_header();
                lets_do_test_mode = true;
            } else {
                rng_seed = get_random_seed();
                write_seed(rng_seed);
            }
            break;
        }
        case SaveFileMode_IGNORE:
            if (cli_syas_test_mode) {
                lets_do_test_mode = true;
            } else {
                rng_seed = get_random_seed();
            }
            break;
    }
}

static Game * parse_snapshot(const List<Token> & first_line_tokens) {
    List<Token> tokens;
    tokens.append_all(first_line_tokens);

    Game * game = create<Game>();
    int token_cursor = 1; // skip the directive

    for (int i = 0; i < WandId_COUNT; i++)
        game->actual_wand_descriptions[i] = (WandDescriptionId)parse_int(tokens[token_cursor++]);
    expect_token(tokens[token_cursor++], ".");
    for (int i = 0; i < PotionId_COUNT; i++)
        game->actual_potion_descriptions[i] = (PotionDescriptionId)parse_int(tokens[token_cursor++]);
    expect_token(tokens[token_cursor++], ".");
    for (int i = 0; i < BookId_COUNT; i++)
        game->actual_book_descriptions[i] = (BookDescriptionId)parse_int(tokens[token_cursor++]);
    expect_token(tokens[token_cursor++], ".");

    game->you_id = parse_uint256(tokens[token_cursor++]);
    game->player_actor_id = parse_uint256(tokens[token_cursor++]);

    game->time_counter = parse_int64(tokens[token_cursor++]);
    int thing_count = parse_int(tokens[token_cursor++]);
    game->dungeon_level = parse_int(tokens[token_cursor++]);

    expect_extra_token_count(tokens, token_cursor - 1);

    // map tiles
    {
        tokens.clear();
        read_tokens(&tokens, false);
        expect_extra_token_count(tokens, 1);
        token_cursor = 0;
        if (parse_directive_id(tokens[token_cursor++].string) != DirectiveId_MAP_TILES)
            report_error(tokens[token_cursor -1], 0, "expected map tiles directive");
        const Token & tiles_token = tokens[token_cursor++];
        int i = 0;
        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++)
            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++)
                game->actual_map_tiles[cursor] = parse_tile_type_short_name(tiles_token, (*tiles_token.string)[i++]);
    }

    // aesthetic indexes
    {
        tokens.clear();
        read_tokens(&tokens, false);
        expect_extra_token_count(tokens, 1);
        token_cursor = 0;
        if (parse_directive_id(tokens[token_cursor++].string) != DirectiveId_AESTHETIC_INDEXES)
            report_error(tokens[token_cursor -1], 0, "expected aesthetic indexes directive");

        const Token & aesthetic_indexes_token = tokens[token_cursor++];
        int i = 0;
        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
                uint32_t high = parse_nibble(aesthetic_indexes_token, i++);
                uint32_t low = parse_nibble(aesthetic_indexes_token, i++);
                game->aesthetic_indexes[cursor] = (high << 4) | low;
            }
        }
    }

    for (int i = 0; i < thing_count; i++) {
        tokens.clear();
        read_tokens(&tokens, false);
        token_cursor = 0;
        if (parse_directive_id(tokens[token_cursor++].string) != DirectiveId_THING)
            report_error(tokens[token_cursor -1], 0, "expected thing directive");

        uint256 id = parse_uint256(tokens[token_cursor++]);
        uint256 container_id = parse_uint256(tokens[token_cursor++]);
        int z_order = parse_int(tokens[token_cursor++]);
        Coord location = parse_coord(tokens[token_cursor], tokens[token_cursor + 1]);
        token_cursor += 2;
        ThingType thing_type = parse_thing_type(tokens[token_cursor++]);
        Thing thing;
        switch (thing_type) {
            case ThingType_INDIVIDUAL: {
                SpeciesId species_id = parse_species_id(tokens[token_cursor++]);
                DecisionMakerType decision_maker = parse_decision_maker(tokens[token_cursor++]);
                thing = create<ThingImpl>(id, species_id, decision_maker);
                break;
            }
            case ThingType_WAND: {
                WandId wand_id = parse_wand_id(tokens[token_cursor++]);
                int charges = parse_int(tokens[token_cursor++]);
                thing = create<ThingImpl>(id, wand_id, charges);
                break;
            }
            case ThingType_POTION: {
                PotionId potion_id = parse_potion_id(tokens[token_cursor++]);
                thing = create<ThingImpl>(id, potion_id);
                break;
            }
            case ThingType_BOOK: {
                BookId book_id = parse_book_id(tokens[token_cursor++]);
                thing = create<ThingImpl>(id, book_id);
                break;
            }

            case ThingType_COUNT:
                unreachable();
        }
        thing->location = location;
        thing->container_id = container_id;
        thing->z_order = z_order;
        game->actual_things.put(id, thing);

        expect_extra_token_count(tokens, token_cursor - 1);
    }
    return game;
}
static void write_snapshot_to_buffer(Game * game, ByteBuffer * buffer) {
    buffer->append(SNAPSHOT_DIRECTIVE);
    for (int i = 0; i < WandId_COUNT; i++)
        buffer->format(" %d", game->actual_wand_descriptions[i]);
    buffer->append(" .");
    for (int i = 0; i < PotionId_COUNT; i++)
        buffer->format(" %d", game->actual_potion_descriptions[i]);
    buffer->append(" .");
    for (int i = 0; i < BookId_COUNT; i++)
        buffer->format(" %d", game->actual_book_descriptions[i]);
    buffer->append(" .");

    buffer->append(' ');
    write_uint_oversized_to_buffer(game->you_id, buffer);
    buffer->append(' ');
    write_uint_oversized_to_buffer(game->player_actor_id, buffer);

    buffer->format(" %" PRIi64, game->time_counter);

    List<Thing> things;
    {
        Thing thing;
        for (auto iterator = game->actual_things.value_iterator(); iterator.next(&thing);)
            things.append(thing);
    }
    sort<Thing, compare_things_by_id>(things.raw(), things.length());
    buffer->format(" %d", things.length());

    buffer->format(" %d", game->dungeon_level);
    buffer->format("\n  %s ", MAP_TILES_DIRECTIVE);
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++)
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++)
            buffer->append(tile_type_short_names[game->actual_map_tiles[cursor]].value);
    buffer->format("\n  %s ", AESTHETIC_INDEXES_DIRECTIVE);
    for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
        for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
            buffer->append(nibble_to_char(game->aesthetic_indexes[cursor] >> 4));
            buffer->append(nibble_to_char(game->aesthetic_indexes[cursor] & 0xf));
        }
    }

    buffer->format("\n  %s %d", RNG_STATE_DIRECTIVE, !!game->test_mode);
    if (game->test_mode) {
        buffer->append(' ');
        write_uint_oversized_to_buffer(game->random_arbitrary_large_number_count, buffer);
        buffer->append(' ');
        write_uint_oversized_to_buffer(game->random_initiative_count, buffer);
    } else {
        buffer->format(" %d", game->the_random_state.index);
        buffer->append(' ');
        for (int i = 0; i < RandomState::ARRAY_SIZE; i++)
            uint32_to_string(game->the_random_state.array[i])->encode(buffer);
    }

    for (int i = 0; i < things.length(); i++) {
        Thing thing = things[i];
        assert(thing->still_exists);

        buffer->format("\n  %s ", THING_DIRECTIVE);
        write_uint_oversized_to_buffer(thing->id, buffer);

        buffer->append(' ');
        write_uint_oversized_to_buffer(thing->container_id, buffer);
        buffer->format(" %d %d %d", thing->z_order, thing->location.x, thing->location.y);

        buffer->append(' ');
        thing_type_names[thing->thing_type]->encode(buffer);

        switch (thing->thing_type) {
            case ThingType_INDIVIDUAL: {
                Life * life = thing->life();
                buffer->append(' ');
                species_names[life->original_species_id]->encode(buffer);
                buffer->append(' ');
                decision_maker_names[life->decision_maker]->encode(buffer);

                buffer->format("\n    %s", LIFE_DIRECTIVE);
                buffer->format(" %d %" PRIu64 " %d %" PRIu64, life->hitpoints, life->hp_regen_deadline, life->mana, life->mp_regen_deadline);
                buffer->format(" %d %" PRIu64 " %" PRIu64, life->experience, life->last_movement_time, life->last_action_time);
                buffer->append(' ');
                write_uint_oversized_to_buffer(life->initiative, buffer);

                List<StatusEffect> status_effects;
                status_effects.append_all(thing->status_effects);
                sort<StatusEffect, compare_status_effects_by_type>(status_effects.raw(), status_effects.length());

                List<AbilityCooldown> ability_cooldowns;
                ability_cooldowns.append_all(thing->ability_cooldowns);
                sort<AbilityCooldown, compare_ability_cooldowns_by_type>(ability_cooldowns.raw(), ability_cooldowns.length());

                buffer->format(" %d %d", status_effects.length(), ability_cooldowns.length());

                for (int i = 0; i < status_effects.length(); i++) {
                    StatusEffect status_effect = status_effects[i];
                    buffer->format("\n      %s %s", STATUS_EFFECT_DIRECTIVE, status_effect_names[status_effect.type].value);
                    buffer->format(" %" PRIi64, status_effect.expiration_time);
                    switch (status_effect.type) {
                        case StatusEffect::CONFUSION:
                        case StatusEffect::SPEED:
                        case StatusEffect::ETHEREAL_VISION:
                        case StatusEffect::COGNISCOPY:
                        case StatusEffect::BLINDNESS:
                        case StatusEffect::INVISIBILITY:
                        case StatusEffect::SLOWING:
                            break;
                        case StatusEffect::POISON:
                            buffer->format(" %" PRIi64, status_effect.poison_next_damage_time);
                            buffer->append(' ');
                            write_uint_oversized_to_buffer(status_effect.who_is_responsible, buffer);
                            break;
                        case StatusEffect::POLYMORPH:
                            buffer->append(' ');
                            species_names[status_effect.species_id]->encode(buffer);
                            break;

                        case StatusEffect::COUNT:
                            unreachable();
                    }
                }
                for (int i = 0; i < ability_cooldowns.length(); i++) {
                    AbilityCooldown ability_cooldown = ability_cooldowns[i];
                    buffer->format("\n      %s", ABILITY_COOLDOWN_DIRECTIVE);
                    buffer->append(' ');
                    ability_names[ability_cooldown.ability_id]->encode(buffer);
                    buffer->format(" %" PRIi64, ability_cooldown.expiration_time);
                }

                Knowledge const& knowledge = life->knowledge;
                buffer->format("\n    %s", KNOWLEDGE_DIRECTIVE);
                for (int i = 0; i < WandDescriptionId_COUNT; i++)
                    buffer->format(" %d", knowledge.wand_identities[i]);
                buffer->append(" .");
                for (int i = 0; i < PotionDescriptionId_COUNT; i++)
                    buffer->format(" %d", knowledge.potion_identities[i]);
                buffer->append(" .");
                for (int i = 0; i < BookDescriptionId_COUNT; i++)
                    buffer->format(" %d", knowledge.book_identities[i]);
                buffer->append(" .");

                List<PerceivedThing> perceived_things;
                {
                    PerceivedThing perceived_thing;
                    for (auto iterator = life->knowledge.perceived_things.value_iterator(); iterator.next(&perceived_thing);)
                        perceived_things.append(perceived_thing);
                }
                sort<PerceivedThing, compare_perceived_things_by_id>(perceived_things.raw(), perceived_things.length());

                buffer->format(" %d", perceived_things.length());

                buffer->format("\n      %s ", MAP_TILES_DIRECTIVE);
                for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++)
                    for (cursor.x = 0; cursor.x < map_size.x; cursor.x++)
                        buffer->append(tile_type_short_names[knowledge.tiles[cursor]].value);

                for (int i = 0; i < perceived_things.length(); i++) {
                    PerceivedThing perceived_thing = perceived_things[i];

                    buffer->format("\n      %s", PERCEIVED_THING_DIRECTIVE);
                    buffer->append(' ');
                    write_uint_oversized_to_buffer(perceived_thing->id, buffer);
                    buffer->format(" %d", !!perceived_thing->is_placeholder);
                    buffer->append(' ');
                    write_uint_oversized_to_buffer(perceived_thing->container_id, buffer);
                    buffer->format(" %d %d %d", perceived_thing->z_order, perceived_thing->location.x, perceived_thing->location.y);

                    buffer->append(' ');
                    thing_type_names[perceived_thing->thing_type]->encode(buffer);
                    switch (perceived_thing->thing_type) {
                        case ThingType_INDIVIDUAL: {
                            buffer->append(' ');
                            species_names[perceived_thing->life()->species_id]->encode(buffer);
                            // TODO: perceived status effects
                            break;
                        }
                        case ThingType_WAND:
                            buffer->append(' ');
                            buffer->append(wand_description_names[perceived_thing->wand_info()->description_id].value);
                            buffer->format(" %d", perceived_thing->wand_info()->used_count);
                            break;
                        case ThingType_POTION:
                            buffer->append(' ');
                            buffer->append(potion_description_names[perceived_thing->potion_info()->description_id].value);
                            break;
                        case ThingType_BOOK:
                            buffer->append(' ');
                            buffer->append(book_description_names[perceived_thing->book_info()->description_id].value);
                            break;

                        case ThingType_COUNT:
                            unreachable();
                    }
                }
                // TODO: rememberd_events
                break;
            }
            case ThingType_WAND:
                buffer->append(' ');
                wand_id_names[thing->wand_info()->wand_id]->encode(buffer);
                buffer->format(" %d", thing->wand_info()->charges);
                break;
            case ThingType_POTION:
                buffer->append(' ');
                potion_id_names[thing->potion_info()->potion_id]->encode(buffer);
                break;
            case ThingType_BOOK:
                buffer->append(' ');
                book_id_names[thing->book_info()->book_id]->encode(buffer);
                break;

            case ThingType_COUNT:
                unreachable();
        }
    }

    assert(game->observer_to_active_identifiable_item.size() == 0);

    buffer->append('\n');
}
static void write_snapshot(Game * game) {
    ByteBuffer buffer;
    write_snapshot_to_buffer(game, &buffer);
    write_buffer(buffer);
}
static void expect_state(Game * expected_state, Game * actual_state) {
    ByteBuffer expected_buffer;
    write_snapshot_to_buffer(expected_state, &expected_buffer);
    ByteBuffer actual_buffer;
    write_snapshot_to_buffer(actual_state, &actual_buffer);
    if (expected_buffer == actual_buffer)
        return;
    fprintf(stderr, "error: corrupt save file\n");
    fprintf(stderr, "\nexpected:\n");
    fprintf(stderr, "%s", expected_buffer.raw());
    fprintf(stderr, "\ngot:\n");
    fprintf(stderr, "%s", actual_buffer.raw());
    exit_with_error();
}

static int test_you_events_mark;
static List<PerceivedThing> test_expect_things_list;
static List<PerceivedThing> test_expect_carrying_list;
static String get_thing_description(const Action::Thing & thing) {
    String result = new_string();
    switch (thing.thing_type) {
        case ThingType_INDIVIDUAL:
            result->format("individual %s", get_species_name_str(thing.species_id));
            return result;
        case ThingType_WAND:
            result->append(get_wand_id_str(thing.wand_id));
            return result;
        case ThingType_POTION:
            result->append(get_potion_id_str(thing.potion_id));
            return result;
        case ThingType_BOOK:
            result->append(get_book_id_str(thing.book_id));
            return result;
        case ThingType_COUNT:
            unreachable();
    }
    unreachable();
}
static PerceivedThing expect_thing(const Action::Thing & expected_thing, List<PerceivedThing> * things_list) {
    if (things_list->length() == 0)
        test_expect_fail("expected %s, found nothing.", get_thing_description(expected_thing));
    for (int i = 0; i < things_list->length(); i++) {
        PerceivedThing thing = (*things_list)[i];
        if (thing->thing_type != expected_thing.thing_type)
            continue;
        switch (thing->thing_type) {
            case ThingType_INDIVIDUAL:
                if (thing->life()->species_id != expected_thing.species_id)
                    continue;
                break;
            case ThingType_WAND:
                if (thing->wand_info()->description_id == WandDescriptionId_UNSEEN) {
                    if (expected_thing.wand_id != WandId_UNKNOWN)
                        continue;
                } else {
                    if (thing->wand_info()->description_id != game->actual_wand_descriptions[expected_thing.wand_id])
                        continue;
                }
                break;
            case ThingType_POTION:
                if (thing->potion_info()->description_id == PotionDescriptionId_UNSEEN) {
                    if (expected_thing.potion_id != PotionId_UNKNOWN)
                        continue;
                } else {
                    if (thing->potion_info()->description_id != game->actual_potion_descriptions[expected_thing.potion_id])
                        continue;
                }
                break;
            case ThingType_BOOK:
                if (thing->book_info()->description_id == BookDescriptionId_UNSEEN) {
                    if (expected_thing.book_id != BookId_UNKNOWN)
                        continue;
                } else {
                    if (thing->book_info()->description_id != game->actual_book_descriptions[expected_thing.book_id])
                        continue;
                }
                break;
            case ThingType_COUNT:
                unreachable();
        }
        // found it
        things_list->swap_remove(i);
        return thing;
    }
    // didn't find it
    test_expect_fail("expected %s, found other things.", get_thing_description(expected_thing));
}
static void expect_nothing(const List<PerceivedThing> & things) {
    if (things.length() == 0)
        return;
    String thing_description = new_string();
    Span span = get_thing_description(you(), things[0]->id);
    span->to_string(thing_description);
    test_expect_fail("expected nothing. found at least: %s.", thing_description);
}
static void skip_blank_events(const List<RememberedEvent> & events) {
    while (test_you_events_mark < events.length() && events[test_you_events_mark] == nullptr)
        test_you_events_mark++;
}

static void handle_directive(DirectiveId directive_id, const List<Token> & tokens) {
    switch (directive_id) {
        case DirectiveId_MARK_EVENTS:
            expect_extra_token_count(tokens, 0);
            test_you_events_mark = you()->life()->knowledge.remembered_events.length();
            break;
        case DirectiveId_EXPECT_EVENT: {
            expect_extra_token_count(tokens, 1);
            const List<RememberedEvent> & events = you()->life()->knowledge.remembered_events;
            skip_blank_events(events);
            if (test_you_events_mark >= events.length())
                test_expect_fail("no new events.");
            const RememberedEvent & event = events[test_you_events_mark];
            String actual_text = new_string();
            event->span->to_string(actual_text);
            String expected_text = parse_string(tokens[1]);
            if (*actual_text != *expected_text)
                test_expect_fail("expected event text \"%s\". got \"%s\".", expected_text, actual_text);
            test_you_events_mark++;
            break;
        }
        case DirectiveId_EXPECT_NO_EVENTS: {
            expect_extra_token_count(tokens, 0);
            const List<RememberedEvent> & events = you()->life()->knowledge.remembered_events;
            skip_blank_events(events);
            if (test_you_events_mark < events.length()) {
                String event_text = new_string();
                events[test_you_events_mark]->span->to_string(event_text);
                test_expect_fail("expected no events. got \"%s\".", event_text);
            }
            break;
        }
        case DirectiveId_FIND_THINGS_AT:
            expect_extra_token_count(tokens, 2);
            test_expect_things_list.clear();
            find_perceived_things_at(you(), parse_coord(tokens[1], tokens[2]), &test_expect_things_list);
            break;
        case DirectiveId_EXPECT_THING: {
            expect_extra_token_count(tokens, 2);
            PerceivedThing thing = expect_thing(parse_thing_signature(tokens[1], tokens[2]), &test_expect_things_list);
            test_expect_carrying_list.clear();
            find_items_in_inventory(you(), thing->id, &test_expect_carrying_list);
            break;
        }
        case DirectiveId_EXPECT_NOTHING:
            expect_extra_token_count(tokens, 0);
            expect_nothing(test_expect_things_list);
            break;
        case DirectiveId_EXPECT_CARRYING:
            expect_extra_token_count(tokens, 2);
            expect_thing(parse_thing_signature(tokens[1], tokens[2]), &test_expect_carrying_list);
            break;
        case DirectiveId_EXPECT_CARRYING_NOTHING:
            expect_extra_token_count(tokens, 0);
            expect_nothing(test_expect_carrying_list);
            break;
        case DirectiveId_SNAPSHOT: {
            // verify the snapshot is correct
            Game * saved_game = parse_snapshot(tokens);
            expect_state(game, saved_game);
            destroy(saved_game, 1);
            break;
        }

        case DirectiveId_MAP_TILES:
        case DirectiveId_AESTHETIC_INDEXES:
        case DirectiveId_RNG_STATE:
        case DirectiveId_THING:
        case DirectiveId_LIFE:
        case DirectiveId_STATUS_EFFECT:
        case DirectiveId_ABILITY_COOLDOWN:
        case DirectiveId_KNOWLEDGE:
        case DirectiveId_PERCEIVED_THING:
            report_error(tokens[0], 0, "directive used out of context");
        case DirectiveId_COUNT:
            unreachable();
    }
}

static Action read_action() {
    List<Token> tokens;
    while (true) {
        read_tokens(&tokens, true);
        if (tokens.length() == 0)
            return Action::undecided(); // EOF

        DirectiveId directive_id = parse_directive_id(tokens[0].string);
        if (directive_id != DirectiveId_COUNT) {
            handle_directive(directive_id, tokens);
            tokens.clear();
            continue;
        } else {
            break;
        }
    }

    Action action;
    action.id = parse_action_id(tokens[0]);
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            if (tokens.length() != 1)
                report_error(tokens[0], 0, "expected no arguments");
            break;
        case Action::Layout_COORD: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            action.coord() = parse_coord(tokens[1], tokens[2]);
            break;
        }
        case Action::Layout_ITEM:
            if (tokens.length() != 2)
                report_error(tokens[0], 0, "expected 1 argument");
            action.item() = parse_uint256(tokens[1]);
            break;
        case Action::Layout_COORD_AND_ITEM: {
            if (tokens.length() != 4)
                report_error(tokens[0], 0, "expected 3 arguments");
            action.coord_and_item().coord = parse_coord(tokens[1], tokens[2]);
            action.coord_and_item().item = parse_uint256(tokens[3]);
            break;
        }
        case Action::Layout_SPECIES: {
            if (tokens.length() != 2)
                report_error(tokens[0], 0, "expected 1 argument");
            action.species() = parse_species_id(tokens[1]);
            break;
        }
        case Action::Layout_THING: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            action.thing() = parse_thing_signature(tokens[1], tokens[2]);
            break;
        }
        case Action::Layout_GENERATE_MONSTER: {
            if (tokens.length() != 5)
                report_error(tokens[0], 0, "expected 4 arguments");
            Action::GenerateMonster & data = action.generate_monster();
            data.species = parse_species_id(tokens[1]);
            data.decision_maker = parse_decision_maker(tokens[2]);
            data.location = parse_coord(tokens[3], tokens[4]);
            break;
        }
        case Action::Layout_ABILITY: {
            if (tokens.length() != 4)
                report_error(tokens[0], 0, "expected 3 arguments");
            Action::AbilityData & data = action.ability();
            data.ability_id = parse_ability_id(tokens[1]);
            data.direction = parse_coord(tokens[2], tokens[3]);
            break;
        }
    }
    // this can catch outdated tests
    if (!validate_action(player_actor(), action)) {
        fprintf(stderr, "%s:%d:1: error: invalid action\n", script_path, line_number);
        exit_with_error();
    }
    return action;
}
static String action_to_string(const Action & action) {
    assert(action.id < Action::Id::COUNT);
    String result = new_string();
    String action_type_string = action_names[action.id];
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            result->format("%s\n", action_type_string);
            break;
        case Action::Layout_COORD: {
            String coord_string1 = int_to_string(action.coord().x);
            String coord_string2 = int_to_string(action.coord().y);
            result->format("%s %s %s\n", action_type_string, coord_string1, coord_string2);
            break;
        }
        case Action::Layout_ITEM:
            result->format("%s %s\n", action_type_string, uint256_to_string(action.item()));
            break;
        case Action::Layout_COORD_AND_ITEM: {
            String coord_string1 = int_to_string(action.coord_and_item().coord.x);
            String coord_string2 = int_to_string(action.coord_and_item().coord.y);
            String item_string = uint256_to_string(action.coord_and_item().item);
            result->format("%s %s %s %s\n", action_type_string, coord_string1, coord_string2, item_string);
            break;
        }
        case Action::Layout_SPECIES: {
            result->format("%s %s\n", action_type_string, species_names[action.species()]);
            break;
        }
        case Action::Layout_THING: {
            const Action::Thing & data = action.thing();
            String thing_type_string = thing_type_names[data.thing_type];
            switch (data.thing_type) {
                case ThingType_INDIVIDUAL: {
                    String species_id_string = species_names[data.species_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, species_id_string);
                    break;
                }
                case ThingType_WAND: {
                    String wand_id_string = wand_id_names[data.wand_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, wand_id_string);
                    break;
                }
                case ThingType_POTION: {
                    String potion_id_string = potion_id_names[data.potion_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, potion_id_string);
                    break;
                }
                case ThingType_BOOK: {
                    String book_id_string = book_id_names[data.book_id];
                    result->format("%s %s %s\n", action_type_string, thing_type_string, book_id_string);
                    break;
                }

                case ThingType_COUNT:
                    unreachable();
            }
            break;
        }
        case Action::Layout_GENERATE_MONSTER: {
            const Action::GenerateMonster & data = action.generate_monster();
            String species_string = species_names[data.species];
            String decision_maker_string = decision_maker_names[data.decision_maker];
            String location_string1 = int_to_string(data.location.x);
            String location_string2 = int_to_string(data.location.y);
            result->format("%s %s %s %s %s\n", action_type_string, species_string, decision_maker_string, location_string1, location_string2);
            break;
        }
        case Action::Layout_ABILITY: {
            const Action::AbilityData & data = action.ability();
            String ability_name = ability_names[data.ability_id];
            String direction_string1 = int_to_string(data.direction.x);
            String direction_string2 = int_to_string(data.direction.y);
            result->format("%s %s %s %s", action_type_string, ability_name,  direction_string1, direction_string2);
            break;
        }
    }
    return result;
}

Action read_decision_from_save_file() {
    if (!headless_mode && replay_delay > 0) {
        if (replay_delay_frame_counter < replay_delay) {
            replay_delay_frame_counter++;
            return Action::undecided(); // let the screen draw
        }
        replay_delay_frame_counter = 0;
    }
    switch (current_mode) {
        case SaveFileMode_READ_WRITE: {
            Action result = read_action();
            if (result.id == Action::UNDECIDED) {
                // end of file
                current_mode = SaveFileMode_WRITE;
            }
            return result;
        }
        case SaveFileMode_READ: {
            Action result = read_action();
            if (result.id == Action::UNDECIDED) {
                // end of file
                fclose(script_file);
                current_mode = SaveFileMode_IGNORE;
            }
            return result;
        }
        case SaveFileMode_WRITE:
        case SaveFileMode_IGNORE:
            // no, you decide.
            return Action::undecided();
    }
    unreachable();
}

void record_decision_to_save_file(const Action & action) {
    switch (current_mode) {
        case SaveFileMode_READ_WRITE:
        case SaveFileMode_READ:
        case SaveFileMode_IGNORE:
            // don't write
            break;
        case SaveFileMode_WRITE:
            if (!game->test_mode)
                write_snapshot(game);
            write_line(action_to_string(action));
            break;
    }
}
int read_rng_input_from_save_file(const ByteBuffer & tag) {
    assert(tag.index_of_rev(' ') == -1);
    int value;

    switch (current_mode) {
        case SaveFileMode_READ_WRITE:
        case SaveFileMode_READ:
            // read from script
            value = read_rng_input(tag);
            break;
        case SaveFileMode_IGNORE:
        case SaveFileMode_WRITE:
            // read from stdio
            printf("%s\n", tag.raw());
            if (scanf("%d", &value) != 1)
                panic("scanf did not return 1");
            break;
    }

    switch (current_mode) {
        case SaveFileMode_READ_WRITE:
        case SaveFileMode_READ:
        case SaveFileMode_IGNORE:
            // don't write
            break;
        case SaveFileMode_WRITE: {
            write_line(rng_input_to_string(tag, value));
            break;
        }
    }

    return value;
}

void delete_save_file() {
    switch (current_mode) {
        case SaveFileMode_READ_WRITE:
        case SaveFileMode_READ:
        case SaveFileMode_IGNORE:
            // don't delete it
            break;
        case SaveFileMode_WRITE:
            fclose(script_file);
            remove(script_path);
            current_mode = SaveFileMode_IGNORE;
            break;
    }
}
