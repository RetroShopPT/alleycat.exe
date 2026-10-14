/*
 * smoke_main.c - headless smoke test: run the translated game (main, CS:0000)
 * on its thread for a number of seconds without a window, then write the
 * 16 KB of emulated CGA memory to argv[1]. Fails if the game thread ends.
 */
#include "bios.h"

#include <stdio.h>
#include <stdlib.h>

#include <SDL2/SDL.h>

static SDL_atomic_t game_finished;

static int game_thread(void *unused)
{
    (void)unused;
    game_main();
    SDL_AtomicSet(&game_finished, 1);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <out.bin> <seconds>\n", argv[0]);
        return 2;
    }
    bios_init();
    SDL_Thread *thread = SDL_CreateThread(game_thread, "game", NULL);
    if (thread == NULL) {
        fprintf(stderr, "SDL_CreateThread failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_DetachThread(thread);
    SDL_Delay((Uint32)atoi(argv[2]) * 1000);
    if (SDL_AtomicGet(&game_finished)) {
        fprintf(stderr, "smoke: game thread ended early\n");
        return 1;
    }

    FILE *f = fopen(argv[1], "wb");
    if (f == NULL) {
        perror(argv[1]);
        return 1;
    }
    if (fwrite(cga_vram, 1, 0x4000, f) != 0x4000) {
        perror(argv[1]);
        fclose(f);
        return 1;
    }
    if (fclose(f) != 0)
        return 1;
    printf("smoke: game ran %s s, VRAM written to %s\n", argv[2], argv[1]);
    return 0;
}
