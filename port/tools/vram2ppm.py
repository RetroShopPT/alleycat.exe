#!/usr/bin/env python3
"""Convert a 16 KB CGA mode 4 memory dump to a PPM image (palette 1, black
background), for viewing `make check` / `make smoke` output offline.

Usage: vram2ppm.py <vram.bin> <out.ppm>
"""
import sys

PALETTE = [(0x00, 0x00, 0x00), (0x00, 0xAA, 0xAA), (0xAA, 0x00, 0xAA), (0xAA, 0xAA, 0xAA)]


def main():
    vram = open(sys.argv[1], 'rb').read()
    out = bytearray()
    for y in range(200):
        start = (y & 1) * 0x2000 + (y >> 1) * 80
        for b in vram[start:start + 80]:
            for shift in (6, 4, 2, 0):
                out += bytes(PALETTE[(b >> shift) & 3])
    open(sys.argv[2], 'wb').write(b'P6 320 200 255\n' + out)


if __name__ == '__main__':
    main()
