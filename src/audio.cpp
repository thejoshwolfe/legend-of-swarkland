#include "audio.hpp"

#include "util.hpp"
#include "resources.hpp"

SDL_AudioDeviceID audio_device;
bool play_the_sound = false;

static int cursor = 0;

void audio_callback(void *, Uint8 * stream, int len) {
    int out_cursor = 0;
    if (play_the_sound) {
        if (cursor < (int)get_binary_audio_attack_resource_size()) {
            out_cursor = min(len, (int)get_binary_audio_attack_resource_size() - cursor);
            memcpy(stream, get_binary_audio_attack_resource_start() + cursor, out_cursor);
            cursor += out_cursor;
        } else {
            play_the_sound = false;
            cursor = 0;
        }
    }
    if (out_cursor < len) {
        memset(stream, 0, len - out_cursor);
    }
}
