/*
 * platform.c - SDL3 front end: window, presenting the emulated CGA screen,
 * events, and the game thread.
 *
 * The translated game runs on its own thread, like the original program on
 * the CPU, while this (main) thread does what the hardware did concurrently:
 * it scans out cga_vram at the display rate and delivers key events to the
 * game's keyboard table (kbd_scancode). The speaker is rendered on SDL's
 * audio thread (speaker.c).
 */
#include "bios.h"
#include "font8x8.h"
#include "speaker.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>  /* WinMain on Windows; no effect on macOS and Linux */

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

/* SDL (USB HID) scancode -> XT set 1 scancode; 0 = no XT equivalent. Keys are
 * mapped by position, as the original reads raw scancodes. The keypad gives
 * the same codes as the arrow/navigation block, as on the XT keyboard. */
static const uint8_t xt_scancode[SDL_SCANCODE_COUNT] = {
    [SDL_SCANCODE_ESCAPE] = 0x01,
    /* 1-4 are aliases for K H T A, the skill keys of the options menu; the
     * game reads no other keys there and ignores digits other than 9. */
    [SDL_SCANCODE_1] = 0x25, [SDL_SCANCODE_2] = 0x23, [SDL_SCANCODE_3] = 0x14,
    [SDL_SCANCODE_4] = 0x1E, [SDL_SCANCODE_5] = 0x06, [SDL_SCANCODE_6] = 0x07,
    [SDL_SCANCODE_7] = 0x08, [SDL_SCANCODE_8] = 0x09, [SDL_SCANCODE_9] = 0x0A,
    [SDL_SCANCODE_0] = 0x0B, [SDL_SCANCODE_MINUS] = 0x0C, [SDL_SCANCODE_EQUALS] = 0x0D,
    [SDL_SCANCODE_BACKSPACE] = 0x0E, [SDL_SCANCODE_TAB] = 0x0F,
    [SDL_SCANCODE_Q] = 0x10, [SDL_SCANCODE_W] = 0x11, [SDL_SCANCODE_E] = 0x12,
    [SDL_SCANCODE_R] = 0x13, [SDL_SCANCODE_T] = 0x14, [SDL_SCANCODE_Y] = 0x15,
    [SDL_SCANCODE_U] = 0x16, [SDL_SCANCODE_I] = 0x17, [SDL_SCANCODE_O] = 0x18,
    [SDL_SCANCODE_P] = 0x19, [SDL_SCANCODE_LEFTBRACKET] = 0x1A, [SDL_SCANCODE_RIGHTBRACKET] = 0x1B,
    [SDL_SCANCODE_RETURN] = 0x1C, [SDL_SCANCODE_KP_ENTER] = 0x1C,
    [SDL_SCANCODE_LCTRL] = 0x1D, [SDL_SCANCODE_RCTRL] = 0x1D,
    [SDL_SCANCODE_A] = 0x1E, [SDL_SCANCODE_S] = 0x1F, [SDL_SCANCODE_D] = 0x20,
    [SDL_SCANCODE_F] = 0x21, [SDL_SCANCODE_G] = 0x22, [SDL_SCANCODE_H] = 0x23,
    [SDL_SCANCODE_J] = 0x24, [SDL_SCANCODE_K] = 0x25, [SDL_SCANCODE_L] = 0x26,
    [SDL_SCANCODE_SEMICOLON] = 0x27, [SDL_SCANCODE_APOSTROPHE] = 0x28, [SDL_SCANCODE_GRAVE] = 0x29,
    [SDL_SCANCODE_LSHIFT] = 0x2A, [SDL_SCANCODE_BACKSLASH] = 0x2B,
    [SDL_SCANCODE_Z] = 0x2C, [SDL_SCANCODE_X] = 0x2D, [SDL_SCANCODE_C] = 0x2E,
    [SDL_SCANCODE_V] = 0x2F, [SDL_SCANCODE_B] = 0x30, [SDL_SCANCODE_N] = 0x31,
    [SDL_SCANCODE_M] = 0x32, [SDL_SCANCODE_COMMA] = 0x33, [SDL_SCANCODE_PERIOD] = 0x34,
    [SDL_SCANCODE_SLASH] = 0x35, [SDL_SCANCODE_RSHIFT] = 0x36, [SDL_SCANCODE_KP_MULTIPLY] = 0x37,
    [SDL_SCANCODE_LALT] = 0x38, [SDL_SCANCODE_RALT] = 0x38, [SDL_SCANCODE_SPACE] = 0x39,
    [SDL_SCANCODE_CAPSLOCK] = 0x3A,
    [SDL_SCANCODE_F1] = 0x3B, [SDL_SCANCODE_F2] = 0x3C, [SDL_SCANCODE_F3] = 0x3D,
    [SDL_SCANCODE_F4] = 0x3E, [SDL_SCANCODE_F5] = 0x3F, [SDL_SCANCODE_F6] = 0x40,
    [SDL_SCANCODE_F7] = 0x41, [SDL_SCANCODE_F8] = 0x42, [SDL_SCANCODE_F9] = 0x43,
    [SDL_SCANCODE_F10] = 0x44,
    [SDL_SCANCODE_HOME] = 0x47, [SDL_SCANCODE_KP_7] = 0x47,
    [SDL_SCANCODE_UP] = 0x48, [SDL_SCANCODE_KP_8] = 0x48,
    [SDL_SCANCODE_PAGEUP] = 0x49, [SDL_SCANCODE_KP_9] = 0x49,
    [SDL_SCANCODE_KP_MINUS] = 0x4A,
    [SDL_SCANCODE_LEFT] = 0x4B, [SDL_SCANCODE_KP_4] = 0x4B,
    [SDL_SCANCODE_KP_5] = 0x4C,
    [SDL_SCANCODE_RIGHT] = 0x4D, [SDL_SCANCODE_KP_6] = 0x4D,
    [SDL_SCANCODE_KP_PLUS] = 0x4E,
    [SDL_SCANCODE_END] = 0x4F, [SDL_SCANCODE_KP_1] = 0x4F,
    [SDL_SCANCODE_DOWN] = 0x50, [SDL_SCANCODE_KP_2] = 0x50,
    [SDL_SCANCODE_PAGEDOWN] = 0x51, [SDL_SCANCODE_KP_3] = 0x51,
    [SDL_SCANCODE_INSERT] = 0x52, [SDL_SCANCODE_KP_0] = 0x52,
    [SDL_SCANCODE_DELETE] = 0x53, [SDL_SCANCODE_KP_PERIOD] = 0x53,
};

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

/* Draws text into pixels with the BIOS font; overlay only, cga_vram is untouched. */
static void draw_text(int x, int y, const char *s, uint32_t color)
{
    for (; *s != '\0'; s++, x += 8) {
        const uint8_t *g = font8x8_glyph((uint8_t)*s);
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (g[r] & (0x80 >> c))
                    pixels[(y + r) * CGA_WIDTH + x + c] = color;
            }
        }
    }
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
    if (game_in_title) {
        static const char version[] = "v" ALLEYCAT_VERSION;
        draw_text(CGA_WIDTH - 8 * ((int)sizeof version - 1), CGA_HEIGHT - 8, version, pal[3]);
    }

    SDL_UpdateTexture(texture, NULL, pixels, CGA_WIDTH * sizeof(uint32_t));
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

/* F10, Ctrl-W and Ctrl-Q quit everywhere; Esc only on the title screen,
 * elsewhere it reaches the game, which pauses ("paws mode"). */
static bool is_quit_key(const SDL_KeyboardEvent *k)
{
    if (k->scancode == SDL_SCANCODE_F10)
        return true;
    if ((k->mod & SDL_KMOD_CTRL) && (k->key == SDLK_W || k->key == SDLK_Q))
        return true;
    return k->scancode == SDL_SCANCODE_ESCAPE && game_in_title;
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
            if (e.type == SDL_EVENT_QUIT) {
                quit = true;
            } else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {
                if (e.type == SDL_EVENT_KEY_DOWN && is_quit_key(&e.key)) {
                    quit = true;
                } else if (xt_scancode[e.key.scancode] != 0) {
                    if (e.key.scancode == SDL_SCANCODE_L)
                        kbd_set_add_life(e.type == SDL_EVENT_KEY_DOWN && (e.key.mod & SDL_KMOD_CTRL));
                    /* Auto-repeat is passed on, like XT typematic make codes. */
                    kbd_scancode(xt_scancode[e.key.scancode] | (e.type == SDL_EVENT_KEY_UP ? 0x80 : 0));
                }
            }
        }
        present();  /* paced by vsync */
    }
    /* The game thread may still be running (it busy-waits like the original);
     * returning from main ends the process. */
    platform_shutdown();
    return 0;
}
