/*
 * font8x8.h - 8x8 font for the emulated BIOS text output (INT 10h/0Eh).
 */
#ifndef ALLEYCAT_FONT8X8_H
#define ALLEYCAT_FONT8X8_H

#include <stdint.h>

/* Builds the glyph table; call once before font8x8_glyph. */
void font8x8_init(void);

/* 8 rows of the glyph for ch, bit 7 = leftmost pixel. */
const uint8_t *font8x8_glyph(uint8_t ch);

#endif
