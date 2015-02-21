#include "string.hpp"

#include <stdarg.h>

void StringImpl::decode(const ByteBuffer &bytes) {
    bool ok;
    decode(bytes, &ok);
    if (!ok)
        panic("invalid UTF-8");
}

void StringImpl::decode(const ByteBuffer &bytes, bool *ok) {
    *ok = true;
    for (int i = 0; i < bytes.length(); i += 1) {
        uint8_t byte1 = *((uint8_t*)&bytes[i]);
        if ((0x80 & byte1) == 0) {
            append(byte1);
        } else if ((0xe0 & byte1) == 0xc0) {
            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x1f);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            append(bits_byte2 | (bits_byte1 << 6));
        } else if ((0xf0 & byte1) == 0xe0) {
            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0xf);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            append(bits_byte3 | (bits_byte2 << 6) | (bits_byte1 << 12));
        } else if ((0xf8 & byte1) == 0xf0) {
            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte2 = *((uint8_t*)&bytes[i]);
            if ((byte2 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte3 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            if (++i >= bytes.length()) {
                *ok = false;
                append(0xfffd);
                break;
            }
            uint8_t byte4 = *((uint8_t*)&bytes[i]);
            if ((byte3 & 0xc0) != 0x80) {
                *ok = false;
                append(0xfffd);
                continue;
            }

            uint32_t bits_byte1 = (byte1 & 0x7);
            uint32_t bits_byte2 = (byte2 & 0x3f);
            uint32_t bits_byte3 = (byte3 & 0x3f);
            uint32_t bits_byte4 = (byte4 & 0x3f);
            append(bits_byte4 | (bits_byte3 << 6) | (bits_byte2 << 12) | (bits_byte1 << 18));
        } else {
            *ok = false;
            append(0xfffd);
        }
    }
}

void StringImpl::encode(ByteBuffer * output_bytes) const {
    for (int i = 0; i < _chars.length(); i += 1) {
        uint32_t codepoint = _chars[i];
        if (codepoint <= 0x7f) {
            // 00000000 00000000 00000000 0xxxxxxx
            unsigned char bytes[1] = {
                (unsigned char)(codepoint),
            };
            output_bytes->append((char*)bytes, 1);
        } else if (codepoint <= 0x7ff) {
            unsigned char bytes[2] = {
                // 00000000 00000000 00000xxx xx000000
                (unsigned char)(0xc0 | (codepoint >> 6)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 2);
        } else if (codepoint <= 0xffff) {
            unsigned char bytes[3] = {
                // 00000000 00000000 xxxx0000 00000000
                (unsigned char)(0xe0 | (codepoint >> 12)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 3);
        } else if (codepoint <= 0x1fffff) {
            unsigned char bytes[4] = {
                // 00000000 000xxx00 00000000 00000000
                (unsigned char)(0xf0 | (codepoint >> 18)),
                // 00000000 000000xx xxxx0000 00000000
                (unsigned char)(0x80 | ((codepoint >> 12) & 0x3f)),
                // 00000000 00000000 0000xxxx xx000000
                (unsigned char)(0x80 | ((codepoint >> 6) & 0x3f)),
                // 00000000 00000000 00000000 00xxxxxx
                (unsigned char)(0x80 | (codepoint & 0x3f)),
            };
            output_bytes->append((char*)bytes, 4);
        } else {
            panic("codepoint out of UTF8 range");
        }
    }
}

String StringImpl::substring(int start, int end) const {
    String result = create<StringImpl>();
    for (int i = start; i < end; i += 1)
        result->append(_chars[i]);
    return result;
}

String StringImpl::substring(int start) const {
    return substring(start, _chars.length());
}

void StringImpl::replace(int start, int end, String s) {
    String second_half = substring(end);
    _chars.resize(_chars.length() + s->length() - (end - start));
    for (int i = 0; i < s->length(); i += 1)
        _chars[start + i] = (*s)[i];
    for (int i = 0; i < second_half->length(); i += 1)
        _chars[start + s->length() + i] = (*second_half)[i];
}

int StringImpl::compare(const StringImpl &a, const StringImpl &b) {
    for (int i = 0;; i += 1) {
        bool a_end = (i >= a.length());
        bool b_end = (i >= b.length());
        if (a_end && b_end) {
            return 0;
        } else if (a_end) {
            return -1;
        } else if (b_end) {
            return 1;
        } else {
            int diff = a[i] - b[i];
            if (diff == 0)
                continue;
            else
                return diff;
        }
    }
}

int StringImpl::compare_insensitive(const StringImpl &a, const StringImpl &b) {
    for (int i = 0;; i += 1) {
        bool a_end = (i >= a.length());
        bool b_end = (i >= b.length());
        if (a_end && b_end) {
            return 0;
        } else if (a_end) {
            return -1;
        } else if (b_end) {
            return 1;
        } else {
            int diff = char_to_lower(a[i]) - char_to_lower(b[i]);
            if (diff == 0)
                continue;
            else
                return diff;
        }
    }
}

void StringImpl::make_lower_case() {
    for (int i = 0; i < _chars.length(); i += 1) {
        _chars[i] = char_to_lower(_chars[i]);
    }
}

void StringImpl::make_upper_case() {
    for (int i = 0; i < _chars.length(); i += 1) {
        _chars[i] = char_to_upper(_chars[i]);
    }
}

uint32_t StringImpl::char_to_lower(uint32_t c) {
    // TODO: unicode support
//    return (c < array_length(unicode_characters)) ? unicode_characters[c].lower : c;
    if ('A' <= c && c <= 'Z')
        return c + ('a' - 'A');
    return c;
}

uint32_t StringImpl::char_to_upper(uint32_t c) {
    // TODO: unicode support
//    return (c < array_length(unicode_characters)) ? unicode_characters[c].upper : c;
    if ('a' <= c && c <= 'z')
        return c - ('a' - 'A');
    return c;
}

bool StringImpl::is_whitespace(uint32_t c) {
    // TODO: unicode support
//    for (size_t i = 0; i < array_length(whitespace); i+= 1) {
//        if (c == whitespace[i])
//            return true;
//    }
//    return false;
    return c == ' ';
}

void StringImpl::split_on_whitespace(List<StringImpl> &out) const {
    out.resize(1);
    StringImpl *current = &out[0];

    for (int i = 0; i < _chars.length(); i += 1) {
        uint32_t c = _chars[i];
        if (is_whitespace(c)) {
            out.resize(out.length() + 1);
            current = &out[out.length() - 1];
            continue;
        }
        current->append(c);
    }
}

int StringImpl::index_of_insensitive(const StringImpl &search) const {
    int upper_bound = _chars.length() - search._chars.length() + 1;
    for (int i = 0; i < upper_bound; i += 1) {
        bool all_ok = true;
        for (int inner = 0; inner < search._chars.length(); inner += 1) {
            uint32_t lower_a = char_to_lower(_chars[i + inner]);
            uint32_t lower_b = char_to_lower(search[inner]);
            if (lower_a != lower_b) {
                all_ok = false;
                break;
            }
        }
        if (all_ok)
            return i;
    }
    return -1;
}
