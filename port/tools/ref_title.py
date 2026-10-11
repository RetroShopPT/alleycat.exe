#!/usr/bin/env python3
"""Independent model of the title backdrop, for `make check`.

Re-implements draw_alley_2a30 (CS:2A30 and callees) plus the six title blits
of title_screen (CS:5CC5..5D0A) directly from the CAT.EXE data, with the
rand16 seed fixed to FA59h, and writes the 16 KB of CGA memory.

Usage: ref_title.py <CAT.EXE> <out.bin>
"""
import struct
import sys

SEED = 0xFA59
TITLE_BLITS = [(0x6152, 0x1d0b, 0x00bd), (0x63d0, 0x160e, 0x069e), (0x6638, 0x0c03, 0x0a78),
               (0x6680, 0x080e, 0x0ca8), (0x6760, 0x0b0c, 0x1d6e), (0x6868, 0x0804, 0x1dec)]


def main():
    exe = open(sys.argv[1], 'rb').read()
    ds = bytearray(exe[0x300:0x300 + 0x7130]) + bytearray(0x10000 - 0x7130)
    vram = bytearray(0x4000)

    def w16(off):
        return struct.unpack_from('<H', ds, off)[0]

    def rand16():
        v = w16(0x2ae5)
        carry = (((v & 0xff) ^ (v >> 8)) >> 1) & 1
        struct.pack_into('<H', ds, 0x2ae5, (v >> 1) | (carry << 15))
        return w16(0x2ae5)

    def blit(si, cx, di, unit=2):
        width, rows = cx & 0xff, cx >> 8
        for _ in range(rows):
            n = width * unit
            vram[di:di + n] = ds[si:si + n]
            si += n
            di ^= 0x2000
            if not di & 0x2000:
                di += 0x50

    def fill(di, value, count):
        for i in range(count):
            struct.pack_into('<H', vram, di + 2 * i, value)

    struct.pack_into('<H', ds, 0x2ae5, SEED)
    fill(0x0000, 0xaaaa, 0xfa0)
    fill(0x2000, 0xaaaa, 0xfa0)

    # fun_2b9e
    last = ds[0x2ac4]
    di = 0x103e
    while True:
        di += 2
        if di >= 0x1090:
            break
        while True:
            dx = rand16() & 0x30
            if dx != last:
                break
        last = dx
        blit(0x2904 + dx, 0x801, di)
    fill(0x1180, 0x5655, 0x500)
    fill(0x3180, 0x5655, 0x500)
    src = 0x2944
    while src < 0x296c:
        for _ in range(9):
            blit(src, 0x501, (rand16() & 0x776) + 0x12c0)
        src += 0xa
    for _ in range(5):
        blit(0x296c, 0x501, (rand16() & 0x3e) + 0x3a98)

    # blit_sprite_list_2b24 with BX = 28A0h, AX = 0
    bx = 0x28a0
    size = w16(bx)
    bx += 2
    while w16(bx) != 0xffff:
        blit(w16(bx), size, w16(bx + 2), unit=1)
        bx += 4

    # fun_2a68 with [6DF8h] = 0
    blit(w16(0x2ad1), 0x801, 0x1902)

    # fun_2c84 with [0008h] = 1
    bx = ds[1 + 0x2ab2]
    while w16(bx + 0x2a86):
        di = w16(bx + 0x2a86)
        middles = 3 if di < 0x1720 else 2
        blit(0x2976, 0xc05, di)
        di += 0x1e0
        for _ in range(middles):
            blit(0x29ee, 0x804, di)
            di += 0x140
        blit(0x2a2e, 0xb04, di)
        bx += 2

    for si, cx, di in TITLE_BLITS:
        blit(si, cx, di)

    open(sys.argv[2], 'wb').write(vram)


if __name__ == '__main__':
    main()
