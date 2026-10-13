# Alley Cat port: CAT.EXE code translated to x86-64 (generated from the Ghidra
# listing re/CAT.asm) + emulated BIOS/hardware + SDL3 front end.
# Builds an x86_64 binary: on macOS it runs under Rosetta 2 on Apple Silicon,
# on Linux it needs an x86_64 host and the system SDL3 (pkg-config sdl3).

BUILD    := build
LISTING  := ../re/CAT.asm
EXE      := ../CAT.EXE
NASM     := nasm
PYTHON   := python3 -I
VER      := 1.0

ifeq ($(shell uname -s),Linux)

CC       := cc
CFLAGS   := -std=c11 -O2 -Wall -Wextra -Isrc $(shell pkg-config --cflags sdl3)
NFLAGS   := -f elf64 -Wall -w-reloc-rel-dword -Isrc/
LDFLAGS  := $(shell pkg-config --libs sdl3)

else

ARCH     := x86_64
SDL_DIR  := third_party
CC       := clang
CFLAGS   := -arch $(ARCH) -std=c11 -O2 -Wall -Wextra -F$(SDL_DIR) -Isrc
# Mach-O C symbols carry a leading underscore; --gprefix adds it to asm globals/externs.
# RIP-relative references across sections are normal on Mach-O, so that warning is off.
NFLAGS   := -f macho64 --gprefix _ -Wall -w-reloc-rel-dword -Isrc/
LDFLAGS  := -arch $(ARCH) -F$(SDL_DIR) -framework SDL3 -Wl,-rpath,@executable_path/../$(SDL_DIR)

endif

GAME_OBJS := $(BUILD)/bios.o $(BUILD)/font8x8.o $(BUILD)/speaker.o $(BUILD)/game.o $(BUILD)/cat_code.o $(BUILD)/data.o
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
$(BUILD)/data.o: src/data.asm src/runtime.inc $(EXE) | $(BUILD)
	$(NASM) $(NFLAGS) $< -o $@

$(BUILD)/%.o: src/%.c src/bios.h src/font8x8.h src/speaker.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# The version is shown on the title screen; rebuild when VER changes.
$(BUILD)/platform.o: CFLAGS += -DALLEYCAT_VERSION='"$(VER)"'
$(BUILD)/platform.o: Makefile

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

# Linux build + headless tests in an x86_64 container (emulated on Apple Silicon).
# The toolchain image is built once and reused; objects go to build/linux.
LINUX_BASE  ?= debian:trixie
LINUX_IMAGE := alleycat-linux-build

linux-image:
	printf 'FROM $(LINUX_BASE)\nRUN apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq make gcc nasm python3 pkg-config libsdl3-dev >/dev/null\n' \
		| docker build --platform linux/amd64 -t $(LINUX_IMAGE) -

LINUX_RUN = docker run --rm --platform linux/amd64 -v "$(abspath ..)":/src -w /src/port \
	-e SDL_VIDEO_DRIVER=dummy -e SDL_AUDIO_DRIVER=dummy $(LINUX_IMAGE) make BUILD=build/linux

linux: linux-image
	$(LINUX_RUN) all

linux-check: linux-image
	$(LINUX_RUN) all check smoke

$(BUILD):
	mkdir -p $@

run: $(TARGET)
	./$(TARGET)

clean:
	-@test -d $(BUILD)/linux && rm -f $(BUILD)/linux/*
	-@test -d $(BUILD)/linux && rmdir $(BUILD)/linux
	@-rm -f $(BUILD)/*
	@-rmdir $(BUILD)

.PHONY: all run check smoke clean linux-image linux linux-check
