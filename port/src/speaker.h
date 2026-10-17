/*
 * speaker.h - PC speaker emulation: PIT channel 2 square wave gated by port
 * 61h, rendered through an SDL2 audio device.
 *
 * The game thread reports every change of the speaker state with a
 * timestamp; the audio thread replays those changes a fixed latency behind
 * real time, so the sound keeps the timing the game produced.
 */
#ifndef ALLEYCAT_SPEAKER_H
#define ALLEYCAT_SPEAKER_H

#include <stdbool.h>
#include <stdint.h>

/* Opens the default playback device. Requires SDL_INIT_AUDIO. */
bool speaker_init(void);
void speaker_shutdown(void);

/* Game thread: new port 61h value (bit 0 = PIT channel 2 gate, bit 1 =
 * speaker data) and channel 2 reload count (0 = 65536), as of t_ns on the
 * host_ns clock. */
void speaker_set(uint64_t t_ns, uint8_t port61, uint16_t pit2_reload);

#endif
