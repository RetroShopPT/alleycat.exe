/*
 * bios.h - emulated PC hardware and BIOS services for the translated game:
 * memory segments, INT 10h/11h/1Ah, port I/O (PIT, speaker gate, CGA,
 * joystick). No window code; usable headless.
 */
#ifndef ALLEYCAT_BIOS_H
#define ALLEYCAT_BIOS_H

#include <stdbool.h>
#include <stdint.h>

#define SEGMENT_SIZE 0x10000

/* Register image exchanged with runtime.inc (BIOS / PORT_* macros).
 * Field offsets are fixed: CPU_AX = 0 ... CPU_PORT = 14. */
struct cpu16 {
    uint16_t ax, bx, cx, dx, si, di, bp, port;
};

extern struct cpu16 cpu;

/* Emulated segments: B800h (CGA), 0040h (BIOS data), 0000h (vectors),
 * F000h (ROM; only the model byte at FFFEh matters). */
extern uint8_t cga_vram[SEGMENT_SIZE];
extern uint8_t bda_seg[SEGMENT_SIZE];
extern uint8_t ivt_seg[SEGMENT_SIZE];
extern uint8_t rom_seg[SEGMENT_SIZE];

void bios_init(void);

/* Host monotonic clock in ns (any thread). */
uint64_t host_ns(void);

/* CGA color select register (port 3D9h) as last set by OUT or INT 10h/0Bh. */
uint8_t bios_cga_color_select(void);

/* Delivers one XT (set 1) scancode, bit 7 = break, as the game's INT 9
 * handler at CS:14B3 would see it on port 60h. Called from the main thread
 * while the game thread runs, like a hardware interrupt. */
void kbd_scancode(uint8_t code);

/* State of the port's Ctrl-L hotkey (both keys held), from the main thread. */
void kbd_set_add_life(bool down);
/* Same for Ctrl-K (toggles infinite lives). */
void kbd_set_infinite_lives(bool down);

/* Called from runtime.inc. */
void bios_int10(void);
void bios_int11(void);
void bios_int1a(void);
uint32_t io_in(void);
void cpu_pace(uint32_t ns);
void port_hotkeys(void);
void io_out(void);

/* Called from game.asm / runtime.inc; none of them return. */
_Noreturn void game_trap(uint32_t cs_offset);
_Noreturn void game_bad_segment(uint32_t segment);

/* 1 while the game runs title_screen (CS:5CB0); set by the generated code. */
extern volatile uint8_t game_in_title;

/* Entry point of the translated game (game.asm). */
void game_main(void);

#endif
