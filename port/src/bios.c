/*
 * bios.c - emulated PC hardware and BIOS services (see bios.h).
 */
#include "bios.h"
#include "font8x8.h"
#include "speaker.h"

#include <stdlib.h>
#include <string.h>

#include <SDL2/SDL.h>

#define PIT_HZ 1193182ULL
#define CGA_FRAME_NS 16683333ULL    /* 59.94 Hz */
#define CGA_VRETRACE_NS 1250000ULL  /* vertical retrace part of each frame */
#define EQUIPMENT_WORD 0x0021       /* diskette, 80x25 colour adapter */
#define MODEL_BYTE_PC 0xFF
#define TEXT_COLS 40                /* 320x200 graphics: 40x25 character cells */
#define TEXT_ROWS 25
#define CGA_ODD_BANK 0x2000
#define CGA_BYTES_PER_ROW 80

/* Keyboard data in the game's data segment (see int09_kbd_isr_pc, CS:14B3). */
#define KBD_PRESS_COUNT 0x0693      /* word, incremented on every make code */
#define KBD_SCANCODES 0x06A1        /* 22 scancodes the game cares about */
#define KBD_STATES 0x06B7           /* state per scancode: 00h down, 80h up */
#define KBD_KEYS 22
#define LIVES 0x1F80                /* byte; shown by CS:26B3 when it changes */
#define MAX_LIVES 9

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

uint64_t host_ns(void)
{
    uint64_t freq = SDL_GetPerformanceFrequency();
    uint64_t c = SDL_GetPerformanceCounter();
    return c / freq * 1000000000ULL + c % freq * 1000000000ULL / freq;
}

static uint64_t elapsed_ns(void)
{
    return host_ns() - start_ns;
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
    start_ns = host_ns();
    font8x8_init();
    rom_seg[0xFFFE] = MODEL_BYTE_PC;
    bda_seg[0x10] = EQUIPMENT_WORD & 0xFF;
    bda_seg[0x11] = EQUIPMENT_WORD >> 8;
}

uint8_t bios_cga_color_select(void)
{
    return cga_color_select;
}

/* Room [0004h] at the last palette change. CS:1D31 sets the palette for each
 * room with INT 10h/0Bh BH=1 (CGA) or INT 10h/10h (PCjr), so latching it there
 * switches colours when the game does, not when [0004h] changes. */
static volatile uint8_t palette_room;

uint8_t bios_pcjr_color(int reg)
{
    /* The game's PCjr palette tables for registers 1-3, one byte per room. */
    static const uint16_t table[3] = { 0x183B, 0x1843, 0x184B };
    return ds_base[table[reg - 1] + (palette_room & 7)] & 0x0F;
}

/* PIT channel 0 as the BIOS programs it: mode 3, reload 65536. In mode 3 the
 * count drops by 2 per input clock, so it sweeps 65536 -> 0 every 27.5 ms. */
static uint16_t pit0_count(void)
{
    uint64_t clocks = elapsed_ns() / 1000 * PIT_HZ / 1000000;
    return (uint16_t)(0x10000 - ((clocks * 2) & 0xFFFF));
}

/* The game thread reads these bytes while this (main) thread writes them.
 * The original ISR ran to completion before the game continued; here the
 * state byte is written before the counter, so a game loop that sees the
 * counter change (e.g. the options menu, CS:5F29) also sees the new state.
 * This thread is the only writer of the counter; a torn read of it only
 * means the game sees the change one poll early. */
void kbd_scancode(uint8_t code)
{
    for (int i = 0; i < KBD_KEYS; i++) {
        if (ds_base[KBD_SCANCODES + i] == (code & 0x7F)) {
            __atomic_store_n(&ds_base[KBD_STATES + i], (uint8_t)(code & 0x80), __ATOMIC_RELEASE);
            break;
        }
    }
    /* On the title screen any counted press leaves it (CS:5DCD); Ctrl, Alt and
     * Shift alone do not count there. */
    uint8_t make = code & 0x7F;
    bool modifier = make == 0x1D || make == 0x38 || make == 0x2A || make == 0x36;
    if (!(code & 0x80) && !(modifier && game_in_title)) {
        uint16_t n = (uint16_t)(ds_base[KBD_PRESS_COUNT] | ds_base[KBD_PRESS_COUNT + 1] << 8) + 1;
        __atomic_store_n(&ds_base[KBD_PRESS_COUNT], (uint8_t)n, __ATOMIC_RELEASE);
        __atomic_store_n(&ds_base[KBD_PRESS_COUNT + 1], (uint8_t)(n >> 8), __ATOMIC_RELEASE);
    }
    /* The original also checks Ctrl-Alt-Del (reboot) and Ctrl-Alt-Up/Down
     * (CRTC screen shift) here (CS:1572); neither is emulated. */
}

static uint8_t *cga_line(int y)
{
    return cga_vram + (y & 1) * CGA_ODD_BANK + (y >> 1) * CGA_BYTES_PER_ROW;
}

/* Draws ch in the cell at the cursor, as the BIOS does in mode 4: set pixels
 * get color (BL bits 0-1), the rest of the cell is cleared; with BL bit 7 the
 * glyph is XORed onto the screen instead. */
static void draw_char(uint8_t ch, uint8_t color)
{
    const uint8_t *glyph = font8x8_glyph(ch);
    for (int r = 0; r < 8; r++) {
        uint16_t pixels = 0;
        for (int x = 0; x < 8; x++) {
            if (glyph[r] & (0x80 >> x))
                pixels |= (uint16_t)(color & 3) << (14 - 2 * x);
        }
        uint8_t *p = cga_line(cursor_row * 8 + r) + cursor_col * 2;
        if (color & 0x80) {
            p[0] ^= pixels >> 8;
            p[1] ^= pixels & 0xFF;
        } else {
            p[0] = pixels >> 8;
            p[1] = pixels & 0xFF;
        }
    }
}

/* Moves the screen up one text row and clears the bottom row. */
static void scroll_up(void)
{
    for (int y = 0; y < (TEXT_ROWS - 1) * 8; y++)
        memcpy(cga_line(y), cga_line(y + 8), CGA_BYTES_PER_ROW);
    for (int y = (TEXT_ROWS - 1) * 8; y < TEXT_ROWS * 8; y++)
        memset(cga_line(y), 0, CGA_BYTES_PER_ROW);
}

/* INT 10h/0Eh teletype output: AL = character, BL = color. */
static void teletype(uint8_t ch, uint8_t color)
{
    switch (ch) {
    case 0x07:  /* bell */
        log_once(0x0E07, "INT 10h/0Eh: bell not emulated");
        return;
    case 0x08:
        if (cursor_col > 0)
            cursor_col--;
        return;
    case 0x0A:
        cursor_row++;
        break;
    case 0x0D:
        cursor_col = 0;
        return;
    default:
        draw_char(ch, color);
        if (++cursor_col == TEXT_COLS) {
            cursor_col = 0;
            cursor_row++;
        }
        break;
    }
    if (cursor_row == TEXT_ROWS) {
        scroll_up();
        cursor_row = TEXT_ROWS - 1;
    }
}

static volatile bool add_life_down;
static volatile bool infinite_lives_down;
static volatile bool invincible_down;

/* 1 while Ctrl-K infinite lives is on: the lives counter is held at 0 (shown
 * as the "unkillable" marker) and the generated code skips its game-over
 * checks. Game thread only. */
uint8_t infinite_lives;

/* 1 while Ctrl-I invincibility is on: the generated code skips the dog's
 * catch test and the knock-downs by the rival cats and the garbage can cat head, and nothing
 * is thrown from the windows. Game thread only. */
uint8_t invincible;

void kbd_set_add_life(bool down)
{
    add_life_down = down;
}

void kbd_set_infinite_lives(bool down)
{
    infinite_lives_down = down;
}

void kbd_set_invincible(bool down)
{
    invincible_down = down;
}

/* Runs at the start of handle_hotkeys (CS:1338), which the game calls only
 * during play: one extra life per Ctrl-L press, up to MAX_LIVES, and
 * Ctrl-K toggles infinite lives. Turning them off restores the count.
 * Ctrl-I toggles invincibility. */
void port_hotkeys(void)
{
    static bool add_applied, toggle_applied, invincible_applied;
    static uint8_t saved_lives;
    if (!infinite_lives_down) {
        toggle_applied = false;
    } else if (!toggle_applied) {
        if (infinite_lives) {
            infinite_lives = 0;
            ds_base[LIVES] = saved_lives;
        } else {
            infinite_lives = 1;
            saved_lives = ds_base[LIVES];
        }
        toggle_applied = true;
    }
    if (!add_life_down) {
        add_applied = false;
    } else if (!add_applied) {
        if (!infinite_lives && ds_base[LIVES] < MAX_LIVES)
            ds_base[LIVES]++;
        add_applied = true;
    }
    if (!invincible_down) {
        invincible_applied = false;
    } else if (!invincible_applied) {
        invincible ^= 1;
        invincible_applied = true;
    }
    /* Held at 0 every frame: also undoes bonus lives, Ctrl-9 and a restart. */
    if (infinite_lives)
        ds_base[LIVES] = 0;
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
        if ((cpu.dx >> 8) >= TEXT_ROWS || (cpu.dx & 0xFF) >= TEXT_COLS)
            log_once(0x0200, "INT 10h/02h: cursor position off screen, clamped");
        cursor_row = (cpu.dx >> 8) < TEXT_ROWS ? cpu.dx >> 8 : TEXT_ROWS - 1;
        cursor_col = (cpu.dx & 0xFF) < TEXT_COLS ? cpu.dx & 0xFF : TEXT_COLS - 1;
        break;
    case 0x0B:  /* set colour palette */
        if (bh == 0)
            cga_color_select = (cga_color_select & 0xE0) | (bl & 0x1F);
        else {
            cga_color_select = (cga_color_select & ~0x20) | ((bl & 1) << 5);
            palette_room = ds_base[4];
        }
        break;
    case 0x0E:  /* teletype output */
        teletype(al, bl);
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
/* Set by speaker port writes (42h, 43h, 61h); cleared by INT 1Ah. Game thread. */
static bool speaker_io;

static uint64_t ticks_at(uint64_t ns)
{
    return ns / 1000 * PIT_HZ / 65536 / 1000000;
}

/* The game waits for the next tick by polling INT 1Ah in a loop, as the
 * original did, which keeps a host core busy. After IDLE_AFTER_POLLS polls of
 * the same tick with no speaker writes in between (those loops make sound),
 * sleep, but wake 2 ms before the next tick and before the next vertical
 * retrace, so the game still sees every tick and every retrace it polls for.
 * The poll count matters: the alley loop (CS:0155) runs the music and most of
 * its logic only on every 4th pass and calls INT 1Ah several times per pass,
 * so it needs many passes per tick, as on the 8088; sleeping on the first
 * repeated poll made the music choppy. */
#define IDLE_MARGIN_NS 2000000ULL
#define IDLE_AFTER_POLLS 500

static void idle_sleep(uint64_t now, uint64_t ticks)
{
    uint64_t next_tick = ((ticks + 1) * 65536 * 1000000000ULL + PIT_HZ - 1) / PIT_HZ;
    uint64_t gap = next_tick - now;
    uint64_t phase = now % CGA_FRAME_NS;
    uint64_t to_retrace = phase < CGA_VRETRACE_NS ? 0 : CGA_FRAME_NS - phase;
    if (to_retrace < gap)
        gap = to_retrace;
    if (gap > IDLE_MARGIN_NS + 1000000ULL)
        SDL_Delay((Uint32)((gap - IDLE_MARGIN_NS) / 1000000ULL));
}

void port_idle(void)
{
    SDL_Delay(1);
}

void bios_int1a(void)
{
    static uint64_t last_ticks = UINT64_MAX;
    static unsigned same_tick_polls;
    if (cpu.ax >> 8 != 0) {
        log_once(0x1A00 | (cpu.ax >> 8), "INT 1Ah: unsupported function");
        return;
    }
    uint64_t now = elapsed_ns();
    uint64_t ticks = ticks_at(now);
    if (ticks != last_ticks) {
        same_tick_polls = 0;
    } else if (!speaker_io && ++same_tick_polls >= IDLE_AFTER_POLLS) {
        idle_sleep(now, ticks);
        ticks = ticks_at(elapsed_ns());
    }
    last_ticks = ticks;
    speaker_io = false;
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
        speaker_io = true;
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
        speaker_io = true;
        if (pit2_write_high) {
            pit2_reload = (pit2_reload & 0x00FF) | al << 8;
            speaker_set(port61, pit2_reload);   /* new count is complete */
        } else {
            pit2_reload = (pit2_reload & 0xFF00) | al;
        }
        pit2_write_high = !pit2_write_high;
        break;
    case 0x61:  /* bit 0 = PIT channel 2 gate, bit 1 = speaker data */
        speaker_io = true;
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

/* Emulates the 8088 speed for the code at the CPU_PACE points (asm2nasm.py):
 * each call takes ns from a virtual CPU clock and busy-waits until real time
 * catches up, so consecutive calls run at the original rate. After a pause
 * longer than PACE_RESYNC_NS the clock restarts from now. */
#define PACE_RESYNC_NS 1000000ULL

void cpu_pace(uint32_t ns)
{
    static uint64_t next;
    uint64_t now = host_ns();
    if (next + PACE_RESYNC_NS < now)
        next = now;
    next += ns;
    while (host_ns() < next)
        ;
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
