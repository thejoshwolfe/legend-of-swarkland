#ifndef AUDIO_HPP
#define AUDIO_HPP

#include <SDL.h>

extern SDL_AudioDeviceID audio_device;
extern bool play_the_sound;

void audio_callback(void *, Uint8 * stream, int len);

#endif
