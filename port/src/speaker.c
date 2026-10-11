/*
 * speaker.c - PC speaker emulation (see speaker.h).
 *
 * Speaker input = (port 61h bit 1) AND (PIT channel 2 output). In mode 3 the
 * channel 2 output is a square wave of 1193182 / reload Hz while its gate
 * (port 61h bit 0) is high, and stays high while the gate is low. So:
 *   level = data && (!gate || square_wave)
 * A DC-blocking filter turns the resulting 0/1 levels into audio.
 */
#include "speaker.h"

#include <stdatomic.h>

#include <SDL3/SDL.h>

#define PIT_HZ 1193182.0
#define SAMPLE_RATE 48000
#define OVERSAMPLE 8
#define LATENCY_NS 40000000ULL      /* replay this far behind real time */
#define RESYNC_NS 150000000ULL      /* larger drift: jump to real time */
#define RING_SIZE (1u << 16)
#define VOLUME 0.25f
#define DC_BLOCK 0.995f

struct speaker_event {
    uint64_t t_ns;
    uint16_t reload;
    uint8_t port61;
};

/* Single producer (game thread), single consumer (audio thread). */
static struct speaker_event ring[RING_SIZE];
static atomic_uint ring_head;
static atomic_uint ring_tail;
static atomic_bool ring_overflow;
static atomic_uint latest_state;     /* port61 | reload << 8, for resync */

static SDL_AudioStream *stream;

/* Audio thread state. */
static uint64_t render_ns;
static uint8_t cur_port61;
static uint16_t cur_reload;
static double phase;                 /* square wave phase, fraction of a cycle */
static float dc_prev_in, dc_prev_out;

void speaker_set(uint8_t port61, uint16_t pit2_reload)
{
    static uint8_t last_port61;
    static uint16_t last_reload;
    port61 &= 3;
    if (port61 == last_port61 && pit2_reload == last_reload)
        return;
    last_port61 = port61;
    last_reload = pit2_reload;
    atomic_store_explicit(&latest_state, port61 | (uint32_t)pit2_reload << 8, memory_order_relaxed);

    unsigned head = atomic_load_explicit(&ring_head, memory_order_relaxed);
    unsigned tail = atomic_load_explicit(&ring_tail, memory_order_acquire);
    if (head - tail == RING_SIZE) {
        atomic_store_explicit(&ring_overflow, true, memory_order_release);
        return;
    }
    ring[head % RING_SIZE] = (struct speaker_event){ SDL_GetTicksNS(), pit2_reload, port61 };
    atomic_store_explicit(&ring_head, head + 1, memory_order_release);
}

static void apply(uint8_t port61, uint16_t reload)
{
    if ((port61 & 1) && !(cur_port61 & 1))
        phase = 0.0;                 /* gate rising edge restarts the count */
    cur_port61 = port61;
    cur_reload = reload;
}

/* Applies all events up to time t. */
static void consume_until(uint64_t t)
{
    unsigned tail = atomic_load_explicit(&ring_tail, memory_order_relaxed);
    unsigned head = atomic_load_explicit(&ring_head, memory_order_acquire);
    while (tail != head && ring[tail % RING_SIZE].t_ns <= t) {
        const struct speaker_event *e = &ring[tail % RING_SIZE];
        apply(e->port61, e->reload);
        tail++;
    }
    atomic_store_explicit(&ring_tail, tail, memory_order_release);
}

static void resync(uint64_t now)
{
    if (atomic_exchange_explicit(&ring_overflow, false, memory_order_acquire)) {
        unsigned head = atomic_load_explicit(&ring_head, memory_order_acquire);
        atomic_store_explicit(&ring_tail, head, memory_order_release);
        uint32_t s = atomic_load_explicit(&latest_state, memory_order_relaxed);
        apply(s & 0xFF, (uint16_t)(s >> 8));
    }
    uint64_t target = now - LATENCY_NS;
    if (render_ns == 0 || render_ns + RESYNC_NS < target || render_ns > target + RESYNC_NS)
        render_ns = target;
}

static float next_sample(void)
{
    const uint64_t step_ns = 1000000000ULL / (SAMPLE_RATE * OVERSAMPLE);
    double freq = PIT_HZ / (cur_reload ? cur_reload : 65536);
    double dphase = freq / (SAMPLE_RATE * OVERSAMPLE);
    int high = 0;
    for (int i = 0; i < OVERSAMPLE; i++) {
        consume_until(render_ns);
        int data = (cur_port61 >> 1) & 1;
        int gate = cur_port61 & 1;
        int square = phase < 0.5;
        high += data && (!gate || square);
        if (gate) {
            phase += dphase;
            phase -= (long)phase;
        }
        render_ns += step_ns;
    }
    float in = (float)high / OVERSAMPLE;
    float out = in - dc_prev_in + DC_BLOCK * dc_prev_out;
    dc_prev_in = in;
    dc_prev_out = out;
    return out * VOLUME;
}

static void SDLCALL audio_callback(void *userdata, SDL_AudioStream *s, int additional_amount, int total_amount)
{
    (void)userdata;
    (void)total_amount;
    float buf[512];
    int samples = additional_amount / (int)sizeof(float);
    resync(SDL_GetTicksNS());
    while (samples > 0) {
        int n = samples < 512 ? samples : 512;
        for (int i = 0; i < n; i++)
            buf[i] = next_sample();
        SDL_PutAudioStreamData(s, buf, n * (int)sizeof(float));
        samples -= n;
    }
}

bool speaker_init(void)
{
    const SDL_AudioSpec spec = { SDL_AUDIO_F32, 1, SAMPLE_RATE };
    stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, NULL);
    if (stream == NULL) {
        SDL_Log("SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ResumeAudioStreamDevice(stream)) {
        SDL_Log("SDL_ResumeAudioStreamDevice failed: %s", SDL_GetError());
        return false;
    }
    return true;
}

void speaker_shutdown(void)
{
    SDL_DestroyAudioStream(stream);
}
