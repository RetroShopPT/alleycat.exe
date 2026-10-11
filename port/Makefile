# Alley Cat port: CAT.EXE code translated to x86-64 (generated from the Ghidra
# listing re/CAT.asm) + emulated BIOS/hardware + SDL3 front end.
# Builds an x86_64 binary that runs under Rosetta 2 on Apple Silicon.

ARCH     := x86_64
SDL_DIR  := third_party
BUILD    := build
LISTING  := ../re/CAT.asm
EXE      := ../CAT.EXE

CC       := clang
CFLAGS   := -arch $(ARCH) -std=c11 -O2 -Wall -Wextra -F$(SDL_DIR) -Isrc
NASM     := nasm
# Mach-O C symbols carry a leading underscore; --gprefix adds it to asm globals/externs.
# RIP-relative references across sections are normal on Mach-O, so that warning is off.
NFLAGS   := -f macho64 --gprefix _ -Wall -w-reloc-rel-dword -Isrc/
LDFLAGS  := -arch $(ARCH) -F$(SDL_DIR) -framework SDL3 -Wl,-rpath,@executable_path/../$(SDL_DIR)
PYTHON   := python3 -I

GAME_OBJS := $(BUILD)/bios.o $(BUILD)/speaker.o $(BUILD)/game.o $(BUILD)/cat_code.o $(BUILD)/data.o
TARGET    := $(BUILD)/alleycat
CHECK     := $(BUILD)/check_title

all: $(TARGET)

$(TARGET): $(BUILD)/platform.o $(GAME_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

$(BUILD)/cat_code.asm: $(LISTING) tools/asm2nasm.py | $(BUILD)
	$(PYTHON) tools/asm2nasm.py $(LISTING) $@

$(BUILD)/cat_code.o: $(BUILD)/cat_code.asm src/runtime.inc
	$(NASM) $(NFLAGS) $< -o $@

$(BUILD)/game.o: src/game.asm src/runtime.inc | $(BUILD)
	$(NASM) $(NFLAGS) $< -o $@

# data.asm incbins ../CAT.EXE relative to this directory.
$(BUILD)/data.o: src/data.asm $(EXE) | $(BUILD)
	$(NASM) $(NFLAGS) $< -o $@

$(BUILD)/%.o: src/%.c src/bios.h src/speaker.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# Headless check: generated backdrop code vs. the independent Python model.
$(BUILD)/check_main.o: tests/check_main.c src/bios.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/check_title.o: tests/check_title.asm src/runtime.inc | $(BUILD)
	$(NASM) $(NFLAGS) $< -o $@

$(CHECK): $(BUILD)/check_main.o $(BUILD)/check_title.o $(GAME_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

check: $(CHECK)
	./$(CHECK) $(BUILD)/vram_game.bin
	$(PYTHON) tools/ref_title.py $(EXE) $(BUILD)/vram_ref.bin
	cmp $(BUILD)/vram_game.bin $(BUILD)/vram_ref.bin && echo "check: title backdrop matches reference"

# Headless smoke test: runs main (CS:0000) for SMOKE_SECONDS without a window.
SMOKE_SECONDS ?= 4

$(BUILD)/smoke_main.o: tests/smoke_main.c src/bios.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/smoke: $(BUILD)/smoke_main.o $(GAME_OBJS)
	$(CC) $^ $(LDFLAGS) -o $@

smoke: $(BUILD)/smoke
	./$(BUILD)/smoke $(BUILD)/vram_smoke.bin $(SMOKE_SECONDS)

$(BUILD):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD)

.PHONY: all run check smoke clean
