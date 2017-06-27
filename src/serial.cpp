#include "serial.hpp"

#include "random.hpp"
#include "display.hpp"

#include <stdio.h>
#include <errno.h>

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
static ByteBuffer read_buffer;
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
                fprintf(stderr, "%s:%d: error: expected newline at end of file\n", script_path, line_number + 1);
                exit_with_error();
            }
            return false;
        }
        read_buffer.append(blob, read_count);
    }
}

class Token {
public:
    int start;
    int end;
};
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
    output_tokens->clear();
    while (output_tokens->length() == 0) {
        output_line->resize(0);
        if (!read_line(output_line)) {
            // EOF
            if (!tolerate_eof) {
                fprintf(stderr, "%s:%d:1: error: unexpected EOF\n", script_path, line_number);
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

static char nibble_to_char(uint32_t nibble) {
    assert(nibble < 0x10);
    return "0123456789abcdef"[nibble];
}
static void write_uint32(ByteBuffer * output_buffer, uint32_t n) {
    // python: hex(n)[2:].zfill(8)
    for (int i = 0; i < 8; i++) {
        uint32_t nibble = (n >> (32 - (i + 1) * 4)) & 0xf;
        char c = nibble_to_char(nibble);
        output_buffer->append(c);
    }
}
template<int Size64>
static void write_uint_oversized_to_buffer(ByteBuffer * output_buffer, uint_oversized<Size64> n) {
    for (int j = 0; j < Size64; j++) {
        for (int i = 0; i < 16; i++) {
            uint32_t nibble = (n.values[j] >> (64 - (i + 1) * 4)) & 0xf;
            char c = nibble_to_char(nibble);
            output_buffer->append(c);
        }
    }
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

static char const* const RNG_DIRECTIVE = "@rng";
static char const* const SEED_DIRECTIVE = "@seed";
static char const* const TEST_MODE_HEADER = "@test";
// TODO: inline these constants
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
static IndexAndValue<ConstStr> constexpr directive_names[DirectiveId_COUNT] {
    {DirectiveId_MARK_EVENTS, "@mark_events"},
    {DirectiveId_EXPECT_EVENT, "@expect_event"},
    {DirectiveId_EXPECT_NO_EVENTS, "@expect_no_events"},
    {DirectiveId_FIND_THINGS_AT, "@find_things_at"},
    {DirectiveId_EXPECT_THING, "@expect_thing"},
    {DirectiveId_EXPECT_NOTHING, "@expect_nothing"},
    {DirectiveId_EXPECT_CARRYING, "@expect_carrying"},
    {DirectiveId_EXPECT_CARRYING_NOTHING, "@expect_carrying_nothing"},
    {DirectiveId_SNAPSHOT, SNAPSHOT_DIRECTIVE},
    {DirectiveId_MAP_TILES, MAP_TILES_DIRECTIVE},
    {DirectiveId_AESTHETIC_INDEXES, AESTHETIC_INDEXES_DIRECTIVE},
    {DirectiveId_RNG_STATE, RNG_STATE_DIRECTIVE},
    {DirectiveId_THING, THING_DIRECTIVE},
    {DirectiveId_LIFE, LIFE_DIRECTIVE},
    {DirectiveId_STATUS_EFFECT, STATUS_EFFECT_DIRECTIVE},
    {DirectiveId_ABILITY_COOLDOWN, ABILITY_COOLDOWN_DIRECTIVE},
    {DirectiveId_KNOWLEDGE, KNOWLEDGE_DIRECTIVE},
    {DirectiveId_PERCEIVED_THING, PERCEIVED_THING_DIRECTIVE},
};
check_indexed_array(directive_names);

static IndexAndValue<ConstStr> constexpr action_names[Action::COUNT] = {
    {Action::MOVE, "move"},
    {Action::WAIT, "wait"},
    {Action::ATTACK, "attack"},
    {Action::ZAP, "zap"},
    {Action::PICKUP, "pickup"},
    {Action::DROP, "drop"},
    {Action::QUAFF, "quaff"},
    {Action::READ_BOOK, "read_book"},
    {Action::THROW, "throw"},
    {Action::ABILITY, "ability"},
    {Action::GO_DOWN, "down"},
    {Action::CHEATCODE_HEALTH_BOOST, "!health"},
    {Action::CHEATCODE_KILL, "!kill"},
    {Action::CHEATCODE_POLYMORPH, "!polymorph"},
    {Action::CHEATCODE_GENERATE_MONSTER, "!monster"},
    {Action::CHEATCODE_WISH, "!wish"},
    {Action::CHEATCODE_IDENTIFY, "!identify"},
    {Action::CHEATCODE_GO_DOWN, "!down"},
    {Action::CHEATCODE_GAIN_LEVEL, "!levelup"},
};
check_indexed_array(action_names);

static IndexAndValue<ConstStr> constexpr species_names[SpeciesId_COUNT + 2] = {
    {SpeciesId_HUMAN, "human"},
    {SpeciesId_OGRE, "ogre"},
    {SpeciesId_LICH, "lich"},
    {SpeciesId_SHAPESHIFTER, "shapeshifter"},
    {SpeciesId_PINK_BLOB, "pink_blob"},
    {SpeciesId_AIR_ELEMENTAL, "air_elemenetal"},
    {SpeciesId_TAR_ELEMENTAL, "tar_elemenetal"},
    {SpeciesId_DOG, "dog"},
    {SpeciesId_ANT, "ant"},
    {SpeciesId_BEE, "bee"},
    {SpeciesId_BEETLE, "beetle"},
    {SpeciesId_SCORPION, "scorpion"},
    {SpeciesId_SNAKE, "snake"},
    {SpeciesId_COBRA, "cobra"},
    {SpeciesId_COUNT, nullptr},
    {SpeciesId_UNSEEN, "unseen"},
};
check_indexed_array(species_names);

static IndexAndValue<ConstStr> constexpr decision_maker_names[DecisionMakerType_COUNT] = {
    {DecisionMakerType_AI, "ai"},
    {DecisionMakerType_PLAYER, "player"},
};
check_indexed_array(decision_maker_names);

static IndexAndValue<ConstStr> constexpr thing_type_names[ThingType_COUNT] = {
    {ThingType_INDIVIDUAL, "individual"},
    {ThingType_WAND, "wand"},
    {ThingType_POTION, "potion"},
    {ThingType_BOOK, "book"},
    {ThingType_WEAPON, "weapon"},
};
check_indexed_array(thing_type_names);

static IndexAndValue<ConstStr> constexpr wand_id_names[WandId_COUNT + 2] = {
    {WandId_WAND_OF_CONFUSION, "confusion"},
    {WandId_WAND_OF_DIGGING, "digging"},
    {WandId_WAND_OF_MAGIC_MISSILE, "magic_missile"},
    {WandId_WAND_OF_SPEED, "speed"},
    {WandId_WAND_OF_REMEDY, "remedy"},
    {WandId_WAND_OF_BLINDING, "blinding"},
    {WandId_WAND_OF_FORCE, "force"},
    {WandId_WAND_OF_INVISIBILITY, "invisibility"},
    {WandId_WAND_OF_MAGIC_BULLET, "magic_bullet"},
    {WandId_WAND_OF_SLOWING, "slowing"},
    {WandId_COUNT, nullptr},
    {WandId_UNKNOWN, "unknown"},
};
check_indexed_array(wand_id_names);

static IndexAndValue<ConstStr> constexpr potion_id_names[PotionId_COUNT + 2] = {
    {PotionId_POTION_OF_HEALING, "healing"},
    {PotionId_POTION_OF_POISON, "poison"},
    {PotionId_POTION_OF_ETHEREAL_VISION, "ethereal_vision"},
    {PotionId_POTION_OF_COGNISCOPY, "cogniscopy"},
    {PotionId_POTION_OF_BLINDNESS, "blindness"},
    {PotionId_POTION_OF_INVISIBILITY, "invisibility"},
    {PotionId_COUNT, nullptr},
    {PotionId_UNKNOWN, "unknown"},
};
check_indexed_array(potion_id_names);

static IndexAndValue<ConstStr> constexpr book_id_names[BookId_COUNT + 2] = {
    {BookId_SPELLBOOK_OF_MAGIC_BULLET, "magic_bullet"},
    {BookId_SPELLBOOK_OF_SPEED, "speed"},
    {BookId_SPELLBOOK_OF_MAPPING, "mapping"},
    {BookId_SPELLBOOK_OF_FORCE, "force"},
    {BookId_SPELLBOOK_OF_ASSUME_FORM, "assume_form"},
    {BookId_COUNT, nullptr},
    {BookId_UNKNOWN, "unknown"},
};
check_indexed_array(book_id_names);

static IndexAndValue<ConstStr> constexpr weapon_id_names[WeaponId_COUNT + 2] = {
    {WeaponId_DAGGER, "dagger"},
    {WeaponId_BATTLEAXE, "battleaxe"},
    {BookId_COUNT, nullptr},
    {BookId_UNKNOWN, "unknown"},
};
check_indexed_array(book_id_names);

static IndexAndValue<ConstStr> constexpr ability_names[AbilityId_COUNT] = {
    {AbilityId_SPIT_BLINDING_VENOM, "spit_blinding_venom"},
    {AbilityId_THROW_TAR, "throw_tar"},
    {AbilityId_ASSUME_FORM, "assume_form"},
};
check_indexed_array(ability_names);

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
};
static_assert(_check_indexed_array(tile_type_short_names, TileType_COUNT), "missed a spot");

static IndexAndValue<ConstStr> constexpr status_effect_names[StatusEffect::COUNT] = {
    {StatusEffect::CONFUSION, "confusion"},
    {StatusEffect::SPEED, "speed"},
    {StatusEffect::ETHEREAL_VISION, "ethereal_vision"},
    {StatusEffect::COGNISCOPY, "cogniscopy"},
    {StatusEffect::BLINDNESS, "blindness"},
    {StatusEffect::INVISIBILITY, "invisibility"},
    {StatusEffect::POISON, "poison"},
    {StatusEffect::POLYMORPH, "polymorph"},
    {StatusEffect::SLOWING, "slowing"},
};
static_assert(_check_indexed_array(status_effect_names, StatusEffect::COUNT), "missed a spot");

static IndexAndValue<ConstStr> constexpr wand_description_names[WandDescriptionId_COUNT + 2] = {
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
    {WandDescriptionId_COUNT, nullptr},
    {WandDescriptionId_UNSEEN, "unseen"},
};
static_assert(_check_indexed_array(wand_description_names, WandDescriptionId_COUNT), "missed a spot");

static IndexAndValue<ConstStr> constexpr potion_description_names[PotionDescriptionId_COUNT + 2] = {
    {PotionDescriptionId_BLUE_POTION, "blue"},
    {PotionDescriptionId_GREEN_POTION, "green"},
    {PotionDescriptionId_RED_POTION, "red"},
    {PotionDescriptionId_YELLOW_POTION, "yellow"},
    {PotionDescriptionId_ORANGE_POTION, "orange"},
    {PotionDescriptionId_PURPLE_POTION, "purple"},
    {PotionDescriptionId_COUNT, nullptr},
    {PotionDescriptionId_UNSEEN, "unseen"},
};
static_assert(_check_indexed_array(potion_description_names, PotionDescriptionId_COUNT), "missed a spot");

static IndexAndValue<ConstStr> constexpr book_description_names[BookDescriptionId_COUNT + 2] = {
    {BookDescriptionId_PURPLE_BOOK, "purple"},
    {BookDescriptionId_BLUE_BOOK, "blue"},
    {BookDescriptionId_RED_BOOK, "red"},
    {BookDescriptionId_GREEN_BOOK, "green"},
    {BookDescriptionId_YELLOW_BOOK, "yellow"},
    {BookDescriptionId_COUNT, nullptr},
    {BookDescriptionId_UNSEEN, "unseen"},
};
static_assert(_check_indexed_array(book_description_names, BookDescriptionId_COUNT), "missed a spot");

template<typename T>
static inline T parse_string_id(IndexAndValue<ConstStr> const* array, int array_length, ByteBuffer const& line, Token const& token) {
    for (int i = 0; i < array_length; i++)
        if (token_equals(line, token, array[i].value))
            return (T)i;
    report_error(token, 0, "undefined identifier");
}

static DirectiveId parse_directive_id(ByteBuffer const& line, Token const& token) {
    for (int i = 0; i < get_array_length(directive_names); i++) {
        if (token_equals(line, token, directive_names[i].value))
            return (DirectiveId)i;
    }
    // nope
    return DirectiveId_COUNT;
}
static Action::Id parse_action_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < Action::COUNT; i++) {
        if (token_equals(line, token, action_names[i].value))
            return (Action::Id)i;
    }
    // this might be a common mistake. special case this i guess
    if (token_equals(line, token, RNG_DIRECTIVE))
        report_error(token, 0, "unexpected rng directive");
    report_error(token, 0, "undefined action name");
}
static SpeciesId parse_species_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(species_names); i++) {
        if (token_equals(line, token, species_names[i].value))
            return (SpeciesId)i;
    }
    report_error(token, 0, "undefined species id");
}
static DecisionMakerType parse_decision_maker(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(decision_maker_names); i++) {
        if (token_equals(line, token, decision_maker_names[i].value))
            return (DecisionMakerType)i;
    }
    report_error(token, 0, "undefined decision maker");
}
static ThingType parse_thing_type(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(thing_type_names); i++) {
        if (token_equals(line, token, thing_type_names[i].value))
            return (ThingType)i;
    }
    report_error(token, 0, "undefined thing type");
}
static WandId parse_wand_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(wand_id_names); i++) {
        if (token_equals(line, token, wand_id_names[i].value))
            return (WandId)i;
    }
    report_error(token, 0, "undefined wand id");
}
static PotionId parse_potion_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(potion_id_names); i++) {
        if (token_equals(line, token, potion_id_names[i].value))
            return (PotionId)i;
    }
    report_error(token, 0, "undefined potion id");
}
static BookId parse_book_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(book_id_names); i++) {
        if (token_equals(line, token, book_id_names[i].value))
            return (BookId)i;
    }
    report_error(token, 0, "undefined book id");
}
static WeaponId parse_weapon_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(weapon_id_names); i++) {
        if (token_equals(line, token, weapon_id_names[i].value))
            return (WeaponId)i;
    }
    report_error(token, 0, "undefined weapon id");
}
static Action::Thing parse_thing_signature(ByteBuffer const& line, const Token & token1, const Token & token2) {
    Action::Thing thing;
    thing.thing_type = parse_thing_type(line, token1);
    switch (thing.thing_type) {
        case ThingType_INDIVIDUAL:
            thing.species_id = parse_species_id(line, token2);
            break;
        case ThingType_WAND:
            thing.wand_id = parse_wand_id(line, token2);
            break;
        case ThingType_POTION:
            thing.potion_id = parse_potion_id(line, token2);
            break;
        case ThingType_BOOK:
            thing.book_id = parse_book_id(line, token2);
            break;
        case ThingType_WEAPON:
            thing.weapon_id = parse_weapon_id(line, token2);
            break;

        case ThingType_COUNT:
            unreachable();
    }
    return thing;
}
static AbilityId parse_ability_id(ByteBuffer const& line, const Token & token) {
    for (int i = 0; i < get_array_length(ability_names); i++) {
        if (token_equals(line, token, ability_names[i].value))
            return (AbilityId)i;
    }
    report_error(token, 0, "undefined ability id");
}
static TileType parse_tile_type_short_name(ByteBuffer const& line, const Token & token, int offset) {
    for (int i = 0; i < get_array_length(tile_type_short_names); i++) {
        if (line[token.start + offset] == tile_type_short_names[i].value)
            return (TileType)i;
    }
    report_error(token, 0, "undefined tile type");
}

static void write_rng_input(ByteBuffer * output_buffer, const ByteBuffer & tag, int value) {
    output_buffer->format("  %s %d ", RNG_DIRECTIVE, value);
    output_buffer->append(tag);
    output_buffer->append("\n");
}
static int read_rng_input(const ByteBuffer & tag) {
    ByteBuffer line;
    List<Token> tokens;
    read_tokens(&line, &tokens, false);
    if (!token_equals(line, tokens[0], RNG_DIRECTIVE))
        report_error(tokens[0], 0, "expected rng directive with tag: ", tag.raw());
    if (tokens.length() != 3)
        report_error(tokens[0], 0, "expected 2 arguments");
    if (!token_equals(line, tokens[2], tag))
        report_error(tokens[2], 0, "rng tag mismatch. expected: ", tag.raw());
    return parse_int(line, tokens[1]);
}

static void write_seed(uint32_t seed) {
    ByteBuffer line;
    line.append(SEED_DIRECTIVE);
    line.append(' ');
    write_uint32(&line, seed);
    line.append('\n');
    write_buffer(line);
}
static void write_test_mode_header() {
    ByteBuffer line;
    line.append(TEST_MODE_HEADER);
    line.append('\n');
    write_buffer(line);
}
static void read_header() {
    ByteBuffer line;
    List<Token> tokens;
    read_tokens(&line, &tokens, false);
    if (token_equals(line, tokens[0], SEED_DIRECTIVE)) {
        if (tokens.length() != 2) {
            fprintf(stderr, "%s:%d:1: error: expected 1 argument", script_path, line_number);
            exit_with_error();
        }
        rng_seed = parse_uint32(line, tokens[1]);
    } else if (token_equals(line, tokens[0], TEST_MODE_HEADER)) {
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

static StatusEffect parse_status_effect() {
    ByteBuffer line;
    List<Token> tokens;
    read_tokens(&line, &tokens, false);
    int token_cursor = 0;
    if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_STATUS_EFFECT)
        report_error(tokens[token_cursor - 1], 0, "expected status effect directive");

    StatusEffect status_effect;
    status_effect.type = parse_string_id<StatusEffect::Id>(status_effect_names, get_array_length(status_effect_names), line, tokens[token_cursor++]);
    status_effect.expiration_time = parse_int64(line, tokens[token_cursor++]);
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
            status_effect.poison_next_damage_time = parse_int64(line, tokens[token_cursor++]);
            status_effect.who_is_responsible = parse_uint256(line, tokens[token_cursor++]);
            break;
        case StatusEffect::POLYMORPH:
            status_effect.species_id = parse_species_id(line, tokens[token_cursor++]);;
            break;

        case StatusEffect::COUNT:
            unreachable();
    }
    expect_extra_token_count(tokens, token_cursor - 1);

    return status_effect;
}
static void write_status_effect(ByteBuffer * output_buffer, StatusEffect status_effect) {
    output_buffer->append(STATUS_EFFECT_DIRECTIVE);
    output_buffer->append(' ');
    output_buffer->append(status_effect_names[status_effect.type].value);
    output_buffer->append(' ');
    output_buffer->format(int64_format, status_effect.expiration_time);
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
            output_buffer->append(' ');
            output_buffer->format(int64_format, status_effect.poison_next_damage_time);
            output_buffer->append(' ');
            write_uint_oversized_to_buffer(output_buffer, status_effect.who_is_responsible);
            break;
        case StatusEffect::POLYMORPH:
            output_buffer->append(' ');
            output_buffer->append(species_names[status_effect.species_id].value);
            break;

        case StatusEffect::COUNT:
            unreachable();
    }
}

static Game * parse_snapshot(ByteBuffer const& first_line, const List<Token> & first_line_tokens) {
    ByteBuffer line;
    line.append(first_line);
    List<Token> tokens;
    tokens.append_all(first_line_tokens);

    Game * game = create<Game>();
    int token_cursor = 1; // skip the directive

    for (int i = 0; i < WandId_COUNT; i++)
        game->actual_wand_descriptions[i] = (WandDescriptionId)parse_int(line, tokens[token_cursor++]);
    expect_token(line, tokens[token_cursor++], ".");
    for (int i = 0; i < PotionId_COUNT; i++)
        game->actual_potion_descriptions[i] = (PotionDescriptionId)parse_int(line, tokens[token_cursor++]);
    expect_token(line, tokens[token_cursor++], ".");
    for (int i = 0; i < BookId_COUNT; i++)
        game->actual_book_descriptions[i] = (BookDescriptionId)parse_int(line, tokens[token_cursor++]);
    expect_token(line, tokens[token_cursor++], ".");

    game->you_id = parse_uint256(line, tokens[token_cursor++]);
    game->player_actor_id = parse_uint256(line, tokens[token_cursor++]);

    game->time_counter = parse_int64(line, tokens[token_cursor++]);
    int thing_count = parse_int(line, tokens[token_cursor++]);
    game->dungeon_level = parse_int(line, tokens[token_cursor++]);

    expect_extra_token_count(tokens, token_cursor - 1);

    // map tiles
    {
        read_tokens(&line, &tokens, false);
        expect_extra_token_count(tokens, 1);
        token_cursor = 0;
        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_MAP_TILES)
            report_error(tokens[token_cursor - 1], 0, "expected map tiles directive");
        const Token & tiles_token = tokens[token_cursor++];
        int i = 0;
        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++)
            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++)
                game->actual_map_tiles[cursor] = parse_tile_type_short_name(line, tiles_token, i++);
    }

    // aesthetic indexes
    {
        read_tokens(&line, &tokens, false);
        expect_extra_token_count(tokens, 1);
        token_cursor = 0;
        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_AESTHETIC_INDEXES)
            report_error(tokens[token_cursor - 1], 0, "expected aesthetic indexes directive");

        const Token & aesthetic_indexes_token = tokens[token_cursor++];
        int i = 0;
        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++) {
            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++) {
                uint32_t high = parse_nibble(line, aesthetic_indexes_token, i++);
                uint32_t low = parse_nibble(line, aesthetic_indexes_token, i++);
                game->aesthetic_indexes[cursor] = (high << 4) | low;
            }
        }
    }

    // rng state
    {
        read_tokens(&line, &tokens, false);
        expect_extra_token_count(tokens, 3);
        token_cursor = 0;
        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_RNG_STATE)
            report_error(tokens[token_cursor - 1], 0, "expected rng state directive");

        game->test_mode = parse_int(line, tokens[token_cursor++]) != 0;
        if (game->test_mode) {
            game->random_arbitrary_large_number_count = parse_uint256(line, tokens[token_cursor++]);
            game->random_initiative_count = parse_uint256(line, tokens[token_cursor++]);
        } else {
            game->the_random_state.index = parse_int(line, tokens[token_cursor++]);
            const Token & rng_state_array_token = tokens[token_cursor++];
            int char_cursor = 0;
            for (int array_cursor = 0; array_cursor < RandomState::ARRAY_SIZE; array_cursor++) {
                game->the_random_state.array[array_cursor] = parse_uint32(line, rng_state_array_token, char_cursor);
                char_cursor += 8;
            }
        }
    }

    for (int i = 0; i < thing_count; i++) {
        read_tokens(&line, &tokens, false);
        token_cursor = 0;
        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_THING)
            report_error(tokens[token_cursor - 1], 0, "expected thing directive");

        uint256 id = parse_uint256(line, tokens[token_cursor++]);
        uint256 container_id = parse_uint256(line, tokens[token_cursor++]);
        int z_order = parse_int(line, tokens[token_cursor++]);
        Coord location = parse_coord(line, tokens[token_cursor], tokens[token_cursor + 1]);
        token_cursor += 2;
        bool still_exists = parse_int(line, tokens[token_cursor++]) != 0;
        ThingType thing_type = parse_thing_type(line, tokens[token_cursor++]);
        Thing thing;
        switch (thing_type) {
            case ThingType_INDIVIDUAL: {
                SpeciesId species_id = parse_species_id(line, tokens[token_cursor++]);
                DecisionMakerType decision_maker = parse_decision_maker(line, tokens[token_cursor++]);
                thing = create<ThingImpl>(id, species_id, decision_maker, uint256::zero());
                expect_extra_token_count(tokens, token_cursor - 1);

                read_tokens(&line, &tokens, false);
                token_cursor = 0;
                if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_LIFE)
                    report_error(tokens[token_cursor - 1], 0, "expected life directive");

                Life * life = thing->life();
                life->hitpoints = parse_int(line, tokens[token_cursor++]);
                life->hp_regen_deadline = parse_int64(line, tokens[token_cursor++]);
                life->mana = parse_int(line, tokens[token_cursor++]);
                life->mp_regen_deadline = parse_int64(line, tokens[token_cursor++]);
                life->experience = parse_int(line, tokens[token_cursor++]);
                life->last_movement_time = parse_int64(line, tokens[token_cursor++]);
                life->last_action_time = parse_int64(line, tokens[token_cursor++]);
                life->initiative = parse_uint256(line, tokens[token_cursor++]);
                int status_effect_count = parse_int(line, tokens[token_cursor++]);
                int ability_cooldowns_count = parse_int(line, tokens[token_cursor++]);
                expect_extra_token_count(tokens, token_cursor - 1);

                for (int i = 0; i < status_effect_count; i++)
                    thing->status_effects.append(parse_status_effect());

                for (int i = 0; i < ability_cooldowns_count; i++) {
                    read_tokens(&line, &tokens, false);
                    expect_extra_token_count(tokens, 2);
                    token_cursor = 0;
                    if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_ABILITY_COOLDOWN)
                        report_error(tokens[token_cursor - 1], 0, "expected ability cooldown directive");

                    AbilityCooldown ability_cooldown;
                    ability_cooldown.ability_id = parse_ability_id(line, tokens[token_cursor++]);
                    ability_cooldown.expiration_time = parse_int64(line, tokens[token_cursor++]);
                    thing->ability_cooldowns.append(ability_cooldown);
                }

                // knowledge
                {
                    read_tokens(&line, &tokens, false);
                    token_cursor = 0;
                    if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_KNOWLEDGE)
                        report_error(tokens[token_cursor - 1], 0, "expected knowledge directive");

                    Knowledge & knowledge = life->knowledge;

                    for (int i = 0; i < WandDescriptionId_COUNT; i++)
                        knowledge.wand_identities[i] = (WandId)parse_int(line, tokens[token_cursor++]);
                    expect_token(line, tokens[token_cursor++], ".");
                    for (int i = 0; i < PotionDescriptionId_COUNT; i++)
                        knowledge.potion_identities[i] = (PotionId)parse_int(line, tokens[token_cursor++]);
                    expect_token(line, tokens[token_cursor++], ".");
                    for (int i = 0; i < BookDescriptionId_COUNT; i++)
                        knowledge.book_identities[i] = (BookId)parse_int(line, tokens[token_cursor++]);
                    expect_token(line, tokens[token_cursor++], ".");

                    int perceived_things_count = parse_int(line, tokens[token_cursor++]);

                    expect_extra_token_count(tokens, token_cursor - 1);

                    // map tiles
                    {
                        read_tokens(&line, &tokens, false);
                        expect_extra_token_count(tokens, 1);
                        token_cursor = 0;
                        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_MAP_TILES)
                            report_error(tokens[token_cursor - 1], 0, "expected map tiles directive");
                        const Token & tiles_token = tokens[token_cursor++];
                        int i = 0;
                        for (Coord cursor = {0, 0}; cursor.y < map_size.y; cursor.y++)
                            for (cursor.x = 0; cursor.x < map_size.x; cursor.x++)
                                knowledge.tiles[cursor] = parse_tile_type_short_name(line, tiles_token, i++);
                    }

                    for (int i = 0; i < perceived_things_count; i++) {
                        read_tokens(&line, &tokens, false);
                        token_cursor = 0;
                        if (parse_directive_id(line, tokens[token_cursor++]) != DirectiveId_PERCEIVED_THING)
                            report_error(tokens[token_cursor - 1], 0, "expected perceived thing directive");

                        uint256 id = parse_uint256(line, tokens[token_cursor++]);
                        bool is_place_holder = parse_int(line, tokens[token_cursor++]) != 0;
                        int64_t last_seen_time = parse_int64(line, tokens[token_cursor++]);
                        uint256 container_id = parse_uint256(line, tokens[token_cursor++]);
                        int z_order = parse_int(line, tokens[token_cursor++]);
                        Coord location = parse_coord(line, tokens[token_cursor], tokens[token_cursor + 1]);
                        token_cursor += 2;
                        ThingType thing_type = parse_thing_type(line, tokens[token_cursor++]);
                        PerceivedThing perceived_thing;
                        switch (thing_type) {
                            case ThingType_INDIVIDUAL: {
                                SpeciesId species_id = parse_species_id(line, tokens[token_cursor++]);
                                perceived_thing = create<PerceivedThingImpl>(id, is_place_holder, species_id, last_seen_time);

                                int perceived_status_effect_count = parse_int(line, tokens[token_cursor++]);
                                expect_extra_token_count(tokens, token_cursor - 1);

                                for (int i = 0; i < perceived_status_effect_count; i++)
                                    perceived_thing->status_effects.append(parse_status_effect());
                                break;
                            }
                            case ThingType_WAND: {
                                WandDescriptionId description_id = parse_string_id<WandDescriptionId>(wand_description_names, get_array_length(wand_description_names), line, tokens[token_cursor++]);
                                int used_count = parse_int(line, tokens[token_cursor++]);
                                perceived_thing = create<PerceivedThingImpl>(id, is_place_holder, description_id, last_seen_time);
                                perceived_thing->wand_info()->used_count = used_count;

                                expect_extra_token_count(tokens, token_cursor - 1);
                                break;
                            }
                            case ThingType_POTION: {
                                PotionDescriptionId description_id = parse_string_id<PotionDescriptionId>(potion_description_names, get_array_length(potion_description_names), line, tokens[token_cursor++]);
                                perceived_thing = create<PerceivedThingImpl>(id, is_place_holder, description_id, last_seen_time);

                                expect_extra_token_count(tokens, token_cursor - 1);
                                break;
                            }
                            case ThingType_BOOK: {
                                BookDescriptionId description_id = parse_string_id<BookDescriptionId>(book_description_names, get_array_length(book_description_names), line, tokens[token_cursor++]);
                                perceived_thing = create<PerceivedThingImpl>(id, is_place_holder, description_id, last_seen_time);

                                expect_extra_token_count(tokens, token_cursor - 1);
                                break;
                            }
                            case ThingType_WEAPON: {
                                WeaponId weapon_id = parse_string_id<WeaponId>(weapon_id_names, get_array_length(weapon_id_names), line, tokens[token_cursor++]);
                                perceived_thing = create<PerceivedThingImpl>(id, is_place_holder, weapon_id, last_seen_time);

                                expect_extra_token_count(tokens, token_cursor - 1);
                                break;
                            }

                            case ThingType_COUNT:
                                unreachable();
                        }
                        perceived_thing->location = location;
                        perceived_thing->container_id = container_id;
                        perceived_thing->z_order = z_order;
                        knowledge.perceived_things.put(id, perceived_thing);
                    }
                }

                break;
            }
            case ThingType_WAND: {
                WandId wand_id = parse_wand_id(line, tokens[token_cursor++]);
                int charges = parse_int(line, tokens[token_cursor++]);
                thing = create<ThingImpl>(id, wand_id, charges);

                expect_extra_token_count(tokens, token_cursor - 1);
                break;
            }
            case ThingType_POTION: {
                PotionId potion_id = parse_potion_id(line, tokens[token_cursor++]);
                thing = create<ThingImpl>(id, potion_id);

                expect_extra_token_count(tokens, token_cursor - 1);
                break;
            }
            case ThingType_BOOK: {
                BookId book_id = parse_book_id(line, tokens[token_cursor++]);
                thing = create<ThingImpl>(id, book_id);

                expect_extra_token_count(tokens, token_cursor - 1);
                break;
            }
            case ThingType_WEAPON: {
                WeaponId weapon_id = parse_weapon_id(line, tokens[token_cursor++]);
                thing = create<ThingImpl>(id, weapon_id);

                expect_extra_token_count(tokens, token_cursor - 1);
                break;
            }

            case ThingType_COUNT:
                unreachable();
        }
        thing->location = location;
        thing->container_id = container_id;
        thing->z_order = z_order;
        thing->still_exists = still_exists;
        game->actual_things.put(id, thing);
    }
    return game;
}
static void write_snapshot_to_buffer(Game const* game, ByteBuffer * buffer) {
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
    write_uint_oversized_to_buffer(buffer, game->you_id);
    buffer->append(' ');
    write_uint_oversized_to_buffer(buffer, game->player_actor_id);

    buffer->append(' ');
    buffer->format(int64_format, game->time_counter);

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
        write_uint_oversized_to_buffer(buffer, game->random_arbitrary_large_number_count);
        buffer->append(' ');
        write_uint_oversized_to_buffer(buffer, game->random_initiative_count);
    } else {
        buffer->format(" %d", game->the_random_state.index);
        buffer->append(' ');
        for (int i = 0; i < RandomState::ARRAY_SIZE; i++)
            write_uint32(buffer, game->the_random_state.array[i]);
    }

    for (int i = 0; i < things.length(); i++) {
        Thing thing = things[i];

        buffer->format("\n  %s ", THING_DIRECTIVE);
        write_uint_oversized_to_buffer(buffer, thing->id);

        buffer->append(' ');
        write_uint_oversized_to_buffer(buffer, thing->container_id);
        buffer->format(" %d %d %d", thing->z_order, thing->location.x, thing->location.y);

        buffer->format(" %d", !!thing->still_exists);

        buffer->append(' ');
        buffer->append(thing_type_names[thing->thing_type].value);

        switch (thing->thing_type) {
            case ThingType_INDIVIDUAL: {
                Life * life = thing->life();
                buffer->append(' ');
                buffer->append(species_names[life->original_species_id].value);
                buffer->append(' ');
                buffer->append(decision_maker_names[life->decision_maker].value);

                buffer->format("\n    %s", LIFE_DIRECTIVE);
                buffer->format(" %d", life->hitpoints);
                buffer->append(' ');
                buffer->format(int64_format, life->hp_regen_deadline);
                buffer->format(" %d", life->mana);
                buffer->append(' ');
                buffer->format(int64_format, life->mp_regen_deadline);
                buffer->format(" %d", life->experience);
                buffer->append(' ');
                buffer->format(int64_format, life->last_movement_time);
                buffer->append(' ');
                buffer->format(int64_format, life->last_action_time);
                buffer->append(' ');
                write_uint_oversized_to_buffer(buffer, life->initiative);

                List<StatusEffect> status_effects;
                status_effects.append_all(thing->status_effects);
                sort<StatusEffect, compare_status_effects_by_type>(status_effects.raw(), status_effects.length());

                List<AbilityCooldown> ability_cooldowns;
                ability_cooldowns.append_all(thing->ability_cooldowns);
                sort<AbilityCooldown, compare_ability_cooldowns_by_type>(ability_cooldowns.raw(), ability_cooldowns.length());

                buffer->format(" %d %d", status_effects.length(), ability_cooldowns.length());

                for (int i = 0; i < status_effects.length(); i++) {
                    StatusEffect status_effect = status_effects[i];
                    buffer->append("\n      ");
                    write_status_effect(buffer, status_effect);
                }
                for (int i = 0; i < ability_cooldowns.length(); i++) {
                    AbilityCooldown ability_cooldown = ability_cooldowns[i];
                    buffer->format("\n      %s", ABILITY_COOLDOWN_DIRECTIVE);
                    buffer->append(' ');
                    buffer->append(ability_names[ability_cooldown.ability_id].value);
                    buffer->append(' ');
                    buffer->format(int64_format, ability_cooldown.expiration_time);
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
                    write_uint_oversized_to_buffer(buffer, perceived_thing->id);
                    buffer->format(" %d", !!perceived_thing->is_placeholder);
                    buffer->append(' ');
                    buffer->format(int64_format, perceived_thing->last_seen_time);
                    buffer->append(' ');
                    write_uint_oversized_to_buffer(buffer, perceived_thing->container_id);
                    buffer->format(" %d %d %d", perceived_thing->z_order, perceived_thing->location.x, perceived_thing->location.y);

                    buffer->append(' ');
                    buffer->append(thing_type_names[perceived_thing->thing_type].value);
                    switch (perceived_thing->thing_type) {
                        case ThingType_INDIVIDUAL: {
                            buffer->append(' ');
                            buffer->append(species_names[perceived_thing->life()->species_id].value);

                            List<StatusEffect> perceived_status_effects;
                            perceived_status_effects.append_all(perceived_thing->status_effects);
                            sort<StatusEffect, compare_status_effects_by_type>(perceived_status_effects.raw(), perceived_status_effects.length());
                            buffer->format(" %d", perceived_status_effects.length());

                            for (int i = 0; i < perceived_status_effects.length(); i++) {
                                buffer->append("\n        ");
                                write_status_effect(buffer, perceived_status_effects[i]);
                            }
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
                        case ThingType_WEAPON:
                            buffer->append(' ');
                            buffer->append(weapon_id_names[perceived_thing->weapon_info()->weapon_id].value);
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
                buffer->append(wand_id_names[thing->wand_info()->wand_id].value);
                buffer->format(" %d", thing->wand_info()->charges);
                break;
            case ThingType_POTION:
                buffer->append(' ');
                buffer->append(potion_id_names[thing->potion_info()->potion_id].value);
                break;
            case ThingType_BOOK:
                buffer->append(' ');
                buffer->append(book_id_names[thing->book_info()->book_id].value);
                break;
            case ThingType_WEAPON:
                buffer->append(' ');
                buffer->append(weapon_id_names[thing->weapon_info()->weapon_id].value);
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
static void expect_state(Game const* expected_state, Game const* actual_state) {
    ByteBuffer expected_buffer;
    write_snapshot_to_buffer(expected_state, &expected_buffer);
    ByteBuffer actual_buffer;
    write_snapshot_to_buffer(actual_state, &actual_buffer);
    if (expected_buffer == actual_buffer)
        return;
    fprintf(stderr, "%s:%d:1: error: corrupt save file\n", script_path, line_number);
    FILE * expected_file = fopen(".expected.swarkland", "wb");
    fwrite(expected_buffer.raw(), expected_buffer.length(), 1, expected_file);
    fclose(expected_file);
    fprintf(stderr, "expected written to .expected.swarkland\n");

    FILE * actual_file = fopen(".actual.swarkland", "wb");
    fwrite(actual_buffer.raw(), actual_buffer.length(), 1, actual_file);
    fclose(actual_file);
    fprintf(stderr, "actual written to .actual.swarkland\n");

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
        case ThingType_WEAPON:
            result->append(get_weapon_id_str(thing.weapon_id));
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
            case ThingType_WEAPON:
                if (thing->weapon_info()->weapon_id == WeaponId_UNKNOWN) {
                    if (expected_thing.weapon_id != WeaponId_UNKNOWN)
                        continue;
                } else {
                    if (thing->weapon_info()->weapon_id != expected_thing.weapon_id)
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

static void handle_directive(DirectiveId directive_id, ByteBuffer const& line, const List<Token> & tokens) {
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
            String expected_text = parse_string(line, tokens[1]);
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
            find_perceived_things_at(you(), parse_coord(line, tokens[1], tokens[2]), &test_expect_things_list);
            break;
        case DirectiveId_EXPECT_THING: {
            expect_extra_token_count(tokens, 2);
            PerceivedThing thing = expect_thing(parse_thing_signature(line, tokens[1], tokens[2]), &test_expect_things_list);
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
            expect_thing(parse_thing_signature(line, tokens[1], tokens[2]), &test_expect_carrying_list);
            break;
        case DirectiveId_EXPECT_CARRYING_NOTHING:
            expect_extra_token_count(tokens, 0);
            expect_nothing(test_expect_carrying_list);
            break;
        case DirectiveId_SNAPSHOT: {
            // verify the snapshot is correct
            Game * saved_game = parse_snapshot(line, tokens);
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
    ByteBuffer line;
    List<Token> tokens;
    while (true) {
        read_tokens(&line, &tokens, true);
        if (tokens.length() == 0)
            return Action::undecided(); // EOF

        DirectiveId directive_id = parse_directive_id(line, tokens[0]);
        if (directive_id != DirectiveId_COUNT) {
            handle_directive(directive_id, line, tokens);
            line.resize(0);
            tokens.clear();
            continue;
        } else {
            break;
        }
    }

    Action action;
    action.id = parse_action_id(line, tokens[0]);
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            if (tokens.length() != 1)
                report_error(tokens[0], 0, "expected no arguments");
            break;
        case Action::Layout_COORD: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            action.coord() = parse_coord(line, tokens[1], tokens[2]);
            break;
        }
        case Action::Layout_ITEM:
            if (tokens.length() != 2)
                report_error(tokens[0], 0, "expected 1 argument");
            action.item() = parse_uint256(line, tokens[1]);
            break;
        case Action::Layout_COORD_AND_ITEM: {
            if (tokens.length() != 4)
                report_error(tokens[0], 0, "expected 3 arguments");
            action.coord_and_item().coord = parse_coord(line, tokens[1], tokens[2]);
            action.coord_and_item().item = parse_uint256(line, tokens[3]);
            break;
        }
        case Action::Layout_SPECIES: {
            if (tokens.length() != 2)
                report_error(tokens[0], 0, "expected 1 argument");
            action.species() = parse_species_id(line, tokens[1]);
            break;
        }
        case Action::Layout_THING: {
            if (tokens.length() != 3)
                report_error(tokens[0], 0, "expected 2 arguments");
            action.thing() = parse_thing_signature(line, tokens[1], tokens[2]);
            break;
        }
        case Action::Layout_GENERATE_MONSTER: {
            if (tokens.length() != 5)
                report_error(tokens[0], 0, "expected 4 arguments");
            Action::GenerateMonster & data = action.generate_monster();
            data.species = parse_species_id(line, tokens[1]);
            data.decision_maker = parse_decision_maker(line, tokens[2]);
            data.location = parse_coord(line, tokens[3], tokens[4]);
            break;
        }
        case Action::Layout_ABILITY: {
            if (tokens.length() != 4)
                report_error(tokens[0], 0, "expected 3 arguments");
            Action::AbilityData & data = action.ability();
            data.ability_id = parse_ability_id(line, tokens[1]);
            data.direction = parse_coord(line, tokens[2], tokens[3]);
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
static void write_action(ByteBuffer * output_buffer, const Action & action) {
    assert(action.id < Action::Id::COUNT);
    output_buffer->append(action_names[action.id].value);
    switch (action.get_layout()) {
        case Action::Layout_VOID:
            break;
        case Action::Layout_COORD:
            output_buffer->format(" %d %d", action.coord().x, action.coord().y);
            break;
        case Action::Layout_ITEM:
            output_buffer->append(' ');
            write_uint_oversized_to_buffer(output_buffer, action.item());
            break;
        case Action::Layout_COORD_AND_ITEM:
            output_buffer->format(" %d %d", action.coord_and_item().coord.x, action.coord_and_item().coord.y);
            output_buffer->append(' ');
            write_uint_oversized_to_buffer(output_buffer, action.coord_and_item().item);
            break;
        case Action::Layout_SPECIES:
            output_buffer->append(' ');
            output_buffer->append(species_names[action.species()].value);
            break;
        case Action::Layout_THING: {
            const Action::Thing & data = action.thing();
            output_buffer->append(' ');
            output_buffer->append(thing_type_names[data.thing_type].value);
            switch (data.thing_type) {
                case ThingType_INDIVIDUAL:
                    output_buffer->append(' ');
                    output_buffer->append(species_names[data.species_id].value);
                    break;
                case ThingType_WAND:
                    output_buffer->append(' ');
                    output_buffer->append(wand_id_names[data.wand_id].value);
                    break;
                case ThingType_POTION:
                    output_buffer->append(' ');
                    output_buffer->append(potion_id_names[data.potion_id].value);
                    break;
                case ThingType_BOOK:
                    output_buffer->append(' ');
                    output_buffer->append(book_id_names[data.book_id].value);
                    break;
                case ThingType_WEAPON:
                    output_buffer->append(' ');
                    output_buffer->append(weapon_id_names[data.weapon_id].value);
                    break;

                case ThingType_COUNT:
                    unreachable();
            }
            break;
        }
        case Action::Layout_GENERATE_MONSTER: {
            const Action::GenerateMonster & data = action.generate_monster();
            output_buffer->append(' ');
            output_buffer->append(species_names[data.species].value);
            output_buffer->append(' ');
            output_buffer->append(decision_maker_names[data.decision_maker].value);
            output_buffer->format(" %d %d", data.location.x, data.location.y);
            break;
        }
        case Action::Layout_ABILITY: {
            const Action::AbilityData & data = action.ability();
            output_buffer->append(' ');
            output_buffer->append(ability_names[data.ability_id].value);
            output_buffer->format(" %d %d", data.direction.x, data.direction.y);
            break;
        }
    }
    output_buffer->append('\n');
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

static int snapshot_counter = 0;
void record_decision_to_save_file(const Action & action) {
    switch (current_mode) {
        case SaveFileMode_READ_WRITE:
        case SaveFileMode_READ:
        case SaveFileMode_IGNORE:
            // don't write
            break;
        case SaveFileMode_WRITE: {
            if (snapshot_interval > 0) {
                snapshot_counter++;
                if (snapshot_counter == snapshot_interval) {
                    write_snapshot(game);
                    snapshot_counter = 0;
                }
            }
            ByteBuffer line;
            write_action(&line, action);
            write_buffer(line);
            break;
        }
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
            ByteBuffer line;
            write_rng_input(&line, tag, value);
            write_buffer(line);
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
