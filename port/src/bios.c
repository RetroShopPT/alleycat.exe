/*
 * bios.c - emulated PC hardware and BIOS services (see bios.h).
 */
#include "bios.h"
#include "speaker.h"

#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#define PIT_HZ 1193182ULL
#define CGA_FRAME_NS 16683333ULL    /* 59.94 Hz */
#define CGA_VRETRACE_NS 1250000ULL  /* vertical retrace part of each frame */
#define EQUIPMENT_WORD 0x0021       /* diskette, 80x25 colour adapter */
#define MODEL_BYTE_PC 0xFF

/* Keyboard data in the game's data segment (see int09_kbd_isr_pc, CS:14B3). */
#define KBD_PRESS_COUNT 0x0693      /* word, incremented on every make code */
#define KBD_SCANCODES 0x06A1        /* 22 scancodes the game cares about */
#define KBD_STATES 0x06B7           /* state per scancode: 00h down, 80h up */
#define KBD_KEYS 22

extern uint8_t ds_base[SEGMENT_SIZE];   /* data.asm */

struct cpu16 cpu;
uint8_t cga_vram[SEGMENT_SIZE];
uint8_t bda_seg[SEGMENT_SIZE];
uint8_t ivt_seg[SEGMENT_SIZE];
uint8_t rom_seg[SEGMENT_SIZE];

static uint64_t start_ns;
static volatile uint8_t cga_color_select = 0x30;
static uint8_t port61;
static uint16_t pit2_reload;
static int pit2_write_high;
static uint16_t pit0_latch;
static int pit0_latched_bytes;      /* bytes of the latch still to be read */
static uint8_t cursor_row, cursor_col;

static uint64_t elapsed_ns(void)
{
    return SDL_GetTicksNS() - start_ns;
}

/* Logs an unsupported request once per key. */
static void log_once(uint32_t key, const char *what)
{
    static uint32_t seen[64];
    static int n;
    for (int i = 0; i < n; i++) {
        if (seen[i] == key)
            return;
    }
    if (n < 64)
        seen[n++] = key;
    SDL_Log("bios: %s (%04X)", what, key & 0xFFFF);
}

void bios_init(void)
{
    start_ns = SDL_GetTicksNS();
    rom_seg[0xFFFE] = MODEL_BYTE_PC;
    bda_seg[0x10] = EQUIPMENT_WORD & 0xFF;
    bda_seg[0x11] = EQUIPMENT_WORD >> 8;
}

uint8_t bios_cga_color_select(void)
{
    return cga_color_select;
}

/* PIT channel 0 as the BIOS programs it: mode 3, reload 65536. In mode 3 the
 * count drops by 2 per input clock, so it sweeps 65536 -> 0 every 27.5 ms. */
static uint16_t pit0_count(void)
{
    uint64_t clocks = elapsed_ns() / 1000 * PIT_HZ / 1000000;
    return (uint16_t)(0x10000 - ((clocks * 2) & 0xFFFF));
}

/* The game thread reads these bytes while this (main) thread writes them, as
 * with the original ISR. This thread is the only writer of the counter; a
 * torn read of it only means the game sees the change one poll early. */
void kbd_scancode(uint8_t code)
{
    if (!(code & 0x80)) {
        uint16_t n = (uint16_t)(ds_base[KBD_PRESS_COUNT] | ds_base[KBD_PRESS_COUNT + 1] << 8) + 1;
        __atomic_store_n(&ds_base[KBD_PRESS_COUNT], (uint8_t)n, __ATOMIC_RELEASE);
        __atomic_store_n(&ds_base[KBD_PRESS_COUNT + 1], (uint8_t)(n >> 8), __ATOMIC_RELEASE);
    }
    for (int i = 0; i < KBD_KEYS; i++) {
        if (ds_base[KBD_SCANCODES + i] == (code & 0x7F)) {
            __atomic_store_n(&ds_base[KBD_STATES + i], (uint8_t)(code & 0x80), __ATOMIC_RELEASE);
            break;
        }
    }
    /* The original also checks Ctrl-Alt-Del (reboot) and Ctrl-Alt-Up/Down
     * (CRTC screen shift) here (CS:1572); neither is emulated. */
}

void bios_int10(void)
{
    uint8_t ah = cpu.ax >> 8, al = cpu.ax & 0xFF;
    uint8_t bh = cpu.bx >> 8, bl = cpu.bx & 0xFF;
    switch (ah) {
    case 0x00:  /* set mode */
        if (al != 4)
            log_once(0x1000 | al, "INT 10h: unsupported video mode");
        memset(cga_vram, 0, 0x4000);
        cga_color_select = 0x30;
        break;
    case 0x02:  /* set cursor position */
        cursor_row = cpu.dx >> 8;
        cursor_col = cpu.dx & 0xFF;
        break;
    case 0x0B:  /* set colour palette */
        if (bh == 0)
            cga_color_select = (cga_color_select & 0xE0) | (bl & 0x1F);
        else
            cga_color_select = (cga_color_select & ~0x20) | ((bl & 1) << 5);
        break;
    case 0x0E:  /* teletype output: needs a font, not implemented yet */
        log_once(0x0E00, "INT 10h/0Eh text output not implemented; text is not drawn");
        break;
    default:
        log_once(0x1000 | ah, "INT 10h: unsupported function");
        break;
    }
}

void bios_int11(void)
{
    cpu.ax = bda_seg[0x10] | bda_seg[0x11] << 8;
}

/* INT 1Ah/00h: ticks since start at 18.2065 Hz in CX:DX, AL = 0 (no midnight). */
void bios_int1a(void)
{
    if (cpu.ax >> 8 != 0) {
        log_once(0x1A00 | (cpu.ax >> 8), "INT 1Ah: unsupported function");
        return;
    }
    uint64_t ticks = elapsed_ns() / 1000 * PIT_HZ / 65536 / 1000000;
    cpu.cx = (uint16_t)(ticks >> 16);
    cpu.dx = (uint16_t)ticks;
    cpu.ax &= 0xFF00;
}

uint32_t io_in(void)
{
    switch (cpu.port) {
    case 0x40:
        if (pit0_latched_bytes == 0) {
            log_once(0x40, "IN 40h without latch");
            return pit0_count() & 0xFF;
        }
        return (pit0_latched_bytes-- == 2) ? (pit0_latch & 0xFF) : (pit0_latch >> 8);
    case 0x61:
        return port61;
    case 0x3DA:  /* CGA status: bit 3 = vertical retrace */
        return (elapsed_ns() % CGA_FRAME_NS < CGA_VRETRACE_NS) ? 0x09 : 0x00;
    case 0x201:  /* no joystick: buttons up, one-shots never time out */
        return 0xFF;
    default:
        log_once(cpu.port, "IN from unsupported port");
        return 0xFF;
    }
}

void io_out(void)
{
    uint8_t al = cpu.ax & 0xFF;
    switch (cpu.port) {
    case 0x43:
        if ((al & 0xF0) == 0x00) {          /* latch channel 0 */
            pit0_latch = pit0_count();
            pit0_latched_bytes = 2;
        } else if (al == 0xB6) {            /* channel 2, low then high byte, mode 3 */
            pit2_write_high = 0;
        } else {
            log_once(0x4300 | al, "OUT 43h: unsupported PIT command");
        }
        break;
    case 0x42:
        if (pit2_write_high) {
            pit2_reload = (pit2_reload & 0x00FF) | al << 8;
            speaker_set(port61, pit2_reload);   /* new count is complete */
        } else {
            pit2_reload = (pit2_reload & 0xFF00) | al;
        }
        pit2_write_high = !pit2_write_high;
        break;
    case 0x61:  /* bit 0 = PIT channel 2 gate, bit 1 = speaker data */
        port61 = al;
        speaker_set(port61, pit2_reload);
        break;
    case 0x3D9:
        cga_color_select = al;
        break;
    case 0x201:  /* joystick one-shot trigger */
    case 0x20:   /* interrupt controller EOI */
        break;
    default:
        log_once(cpu.port, "OUT to unsupported port");
        break;
    }
}

_Noreturn void game_trap(uint32_t cs_offset)
{
    SDL_Log("game: untranslated instruction reached at CS:%04X", cs_offset);
    abort();
}

_Noreturn void game_bad_segment(uint32_t segment)
{
    SDL_Log("game: unknown segment value %04X loaded", segment);
    abort();
}

_Noreturn void game_park(void)
{
    SDL_Log("game: options_menu (CS:5EE5) reached; not translated yet, game thread parked");
    for (;;)
        SDL_Delay(1000);
}
