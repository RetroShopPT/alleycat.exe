/*
 * check_main.c - headless check: run the generated title backdrop code and
 * write the 16 KB of emulated CGA memory to argv[1].
 */
#include "bios.h"

#include <stdio.h>

void check_title(void);

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s <out.bin>\n", argv[0]);
        return 2;
    }
    bios_init();
    check_title();

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
    return fclose(f) == 0 ? 0 : 1;
}
