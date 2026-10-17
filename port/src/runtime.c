/*
 * runtime.c - hand-written parts of the C translation (web build): CPU
 * state, segment lookup, the port's help strings and the entry point. The
 * C counterpart of game.asm; the game code itself is generated into
 * cat_code.c by tools/asm2c.py.
 */
#include "runtime.h"

#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

uint8_t cf, zf, sf, of, af, df;
uint8_t *ds, *es;
uint16_t seg_ds, seg_es;
uint8_t cs_seg[SEGMENT_SIZE];       /* code-segment variables (CS:13DF..13E7) */
uint32_t stack[STACK_ENTRIES];
unsigned sp;
uint32_t pc;
volatile uint8_t game_yield;
volatile uint8_t game_in_title;

/* Segment values seen in the listing: 1010h (the data segment after
 * relocation), B800h, 0040h, 0000h, F000h. */
uint8_t *seg_to_base(uint16_t seg)
{
    switch (seg) {
    case 0x1010: return ds_base;
    case 0xB800: return cga_vram;
    case 0x0040: return bda_seg;
    case 0x0000: return ivt_seg;
    case 0xF000: return rom_seg;
    default: game_bad_segment(seg);
    }
}

_Noreturn void stack_fault(void)
{
    SDL_Log("game: stack over- or underflow at CS:%04X", (unsigned)pc);
    abort();
}

static const struct {
    uint16_t offset;
    const char *text;
} help_lines[] = {
    { HELP_CTRL_L, "   Ctrl-L  adds a life (up to 9)." },
    { HELP_CTRL_I, "   Ctrl-I  toggles invincibility." },
    { HELP_CTRL_P, "   Ctrl-P  toggles PCjr colors." },
    { HELP_FULLSCREEN, "   Alt-Enter toggles fullscreen." },
    { HELP_CTRL_R, "   Ctrl-R  returns to the title screen." },
};

/* Sets up main (CS:0000) as game_main in game.asm does: DS = ES = the data
 * segment, registers zero. A return address that no CALL pushes ends the
 * game if main ever returns. */
void game_start(void)
{
    for (size_t i = 0; i < sizeof help_lines / sizeof help_lines[0]; i++)
        strcpy((char *)ds_base + help_lines[i].offset, help_lines[i].text);
    memset(&cpu, 0, sizeof cpu);
    set_ds(0x1010);
    set_es(0x1010);
    sp = 0;
    push(UINT32_MAX);
    pc = 0x0000;
}

/* Runs the game to its end on the calling thread (non-web C builds). */
void game_main(void)
{
    game_start();
    while (game_run())
        game_yield = 0;
}
