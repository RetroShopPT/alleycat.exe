/*
 * platform.c - SDL3 front end: window, presenting the emulated CGA screen,
 * events, and the game thread.
 *
 * The translated game runs on its own thread, like the original program on
 * the CPU, while this (main) thread does what the hardware did concurrently:
 * it scans out cga_vram at the display rate and handles input. The speaker
 * is rendered on SDL's audio thread (speaker.c).
 */
#include "bios.h"
#include "speaker.h"

#include <SDL3/SDL.h>

#define CGA_WIDTH 320
#define CGA_HEIGHT 200
#define CGA_BYTES_PER_ROW 80
#define CGA_ODD_BANK 0x2000
#define WINDOW_SCALE 4

static SDL_Window *window;
static SDL_Renderer *renderer;
static SDL_Texture *texture;
static uint32_t pixels[CGA_WIDTH * CGA_HEIGHT];
static SDL_AtomicInt game_finished;

/* The 16 RGBI colors of a CGA monitor, as XRGB8888. */
static const uint32_t cga_rgbi[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA, 0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF, 0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF,
};

static void build_palette(uint32_t out[4])
{
    uint8_t sel = bios_cga_color_select();
    int bright = (sel & 0x10) ? 8 : 0;
    out[0] = cga_rgbi[sel & 0x0F];
    if (sel & 0x20) {
        out[1] = cga_rgbi[3 + bright];  /* cyan */
        out[2] = cga_rgbi[5 + bright];  /* magenta */
        out[3] = cga_rgbi[7 + bright];  /* white */
    } else {
        out[1] = cga_rgbi[2 + bright];  /* green */
        out[2] = cga_rgbi[4 + bright];  /* red */
        out[3] = cga_rgbi[6 + bright];  /* brown/yellow */
    }
}

static bool platform_init(void)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_CreateWindowAndRenderer("Alley Cat", CGA_WIDTH * WINDOW_SCALE, CGA_HEIGHT * WINDOW_SCALE,
                                     SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetRenderLogicalPresentation(renderer, CGA_WIDTH, CGA_HEIGHT,
                                          SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
        SDL_Log("SDL_SetRenderLogicalPresentation failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
                                CGA_WIDTH, CGA_HEIGHT);
    if (texture == NULL) {
        SDL_Log("SDL_CreateTexture failed: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    if (!speaker_init())
        SDL_Log("continuing without sound");
    return true;
}

static void platform_shutdown(void)
{
    speaker_shutdown();
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

/* Converts cga_vram (320x200, 2 bpp, interlaced) to RGB and shows it. */
static void present(void)
{
    uint32_t pal[4];
    build_palette(pal);

    for (int y = 0; y < CGA_HEIGHT; y++) {
        const uint8_t *row = cga_vram + (y & 1) * CGA_ODD_BANK + (y >> 1) * CGA_BYTES_PER_ROW;
        uint32_t *out = pixels + y * CGA_WIDTH;
        for (int x = 0; x < CGA_BYTES_PER_ROW; x++) {
            uint8_t b = row[x];
            out[0] = pal[(b >> 6) & 3];
            out[1] = pal[(b >> 4) & 3];
            out[2] = pal[(b >> 2) & 3];
            out[3] = pal[b & 3];
            out += 4;
        }
    }

    SDL_UpdateTexture(texture, NULL, pixels, CGA_WIDTH * sizeof(uint32_t));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

static int game_thread(void *unused)
{
    (void)unused;
    game_main();
    SDL_SetAtomicInt(&game_finished, 1);
    return 0;
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (!platform_init())
        return 1;
    bios_init();

    SDL_Thread *thread = SDL_CreateThread(game_thread, "game", NULL);
    if (thread == NULL) {
        SDL_Log("SDL_CreateThread failed: %s", SDL_GetError());
        platform_shutdown();
        return 1;
    }
    SDL_DetachThread(thread);

    bool quit = false;
    while (!quit && !SDL_GetAtomicInt(&game_finished)) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT)
                quit = true;
        }
        present();  /* paced by vsync */
    }
    /* The game thread may still be running (it busy-waits like the original);
     * returning from main ends the process. */
    platform_shutdown();
    return 0;
}
