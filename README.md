# Alley Cat (port)

A port of [Alley Cat](https://wikipedia.org/wiki/Alley_Cat_(video_game)) (Bill Williams, SynSoft/IBM, 1984) from its original DOS executable to `x86_64` platforms. The game was written in hand-coded 8088 assembly. This project disassembles `CAT.EXE`, translates the 16-bit code into x86-64 assembly, and runs it on emulated PC hardware with SDL3 handling video and sound.

> [!IMPORTANT]
> 99.99% LLM-generated code. I have rougly reviewed the output the agent produced, but if you have your concerns about AI, you should abstain from using this project.

![Alley Cat Title Screen](screenshots/title-screen.png)

## Status

Working:
- the full title sequence (backdrop, animated attract mode with the cat)
- the title music and timer-driven sound effects through the emulated PC speaker
- keyboard input
- the options menu, with its text drawn in a built-in 8x8 font

Not yet tested:
- Gameplay past the start of the first level.
- Two sound effects that toggle the speaker in software. They are paced to an estimated 8088 speed and may sound too high or too low.

Todo:
- Joystick input
- Linux build
- Windows build

## Controls

- Cursor keys move the cat. Alt performs special actions.
- In the options menu, K, H, T and A (or 1 to 4) choose the skill level: `Kitten`, `House Cat`, `Tomcat`, `Alley Cat`. The skill sets the starting difficulty; the game raises it as you complete levels.
- `Ctrl-S` turns the sound on and off, `Ctrl-R` restarts the game, `Ctrl-M` returns to the options menu.
- `Ctrl-L` adds a life, up to `9`. This is not in the original game.
- `Esc` pauses the game. On the title screen it quits.
- `F10`, `Ctrl-W` and `Ctrl-Q` quit at any time.

## Requirements

- macOS with Rosetta 2. The port is an x86_64 binary.
- Xcode command line tools (clang, make)
- nasm (`brew install nasm`)
- Python 3. It is used at build time to generate the translated code.
- SDL3, as the official macOS release framework (3.4.x, universal). Copy `SDL3.framework` to `port/third_party/SDL3.framework`.
- The original `CAT.EXE`, in the repository root. The build reads the game data directly from it. `CAT.EXE` and the disassembly derived from it are copyrighted by their original owners.

## Building and running

From `port/`:

- `make` builds `build/alleycat`
- `make run` builds and starts the game
- `make check` is a headless test. It runs the translated title backdrop code with a fixed random seed and compares the resulting screen memory with an independent Python model (`tools/ref_title.py`).
- `make smoke` is a headless test. It runs the game without a window for a few seconds and writes the screen memory to `build/vram_smoke.bin`. Set the duration with `SMOKE_SECONDS=20`. Convert the dump to an image with `tools/vram2ppm.py`.
- `make clean` removes `build/`

Messages about unsupported BIOS calls or I/O ports are printed once each to the terminal.

## Layout

- `re/` holds the reverse-engineering material.
  - `re/CAT.asm` is the Ghidra listing of the code segment. `<NOT INCLUDED>`
  - `re/DumpListing.java` is the Ghidra script that produces it.
  - `re/NOTES.md` covers the memory layout, subsystems, key table, hotkeys and open questions.
- `port/tools/asm2nasm.py` translates `re/CAT.asm` into `port/build/cat_code.asm`. Every original instruction gets a label `L_xxxx` after its code-segment offset. Its `MARKERS` table holds the port's few changes to the game code (skipped joystick question, Ctrl-L, the extra help line), and `CPU_PACE_NS` slows the two software-driven sound loops to the original speed.
- `port/src/runtime.inc` holds the execution model: how 8086 registers, segments, string instructions, flags and BIOS/port calls map to x86-64. The generated code and the hand-written code both use it.
- `port/src/game.asm` holds the hand-written parts: the entry and exit, the segment lookup, and the code ranges the generator skips (interrupt vector setup and the exit path).
- `port/src/data.asm` includes the original data segment straight from `CAT.EXE`, so data offsets match the listing. All game text, including the menu strings, comes from there. Text added by the port is stored past the end of the original data.
- `port/src/bios.c` emulates the hardware: the BIOS services (INT 10h, 11h, 1Ah), the timer chip, the CGA registers, the speaker ports, the keyboard and the memory segments.
- `port/src/font8x8.c` is the 8x8 font for BIOS text output, drawn for this port in CGA style.
- `port/src/speaker.c` emulates the PC speaker on an SDL audio stream.
- `port/src/platform.c` is the SDL front end: the window, display of the CGA screen, keyboard events and the game thread.
- `port/tests/` holds the headless test programs used by `make check` and `make smoke`.

## How it works

The original program is translated, not emulated instruction by instruction. The 8086 registers keep their 16-bit names in the generated code. DS and ES become base pointers into emulated 64 KB segments. Interrupts and port accesses become calls into C.

The game runs on its own thread, busy-waiting on the timer as it did on the original PC. The main thread plays the part of the display hardware: it converts the emulated CGA memory to pixels at 60 Hz, handles window events, and writes key presses into the game's key table, as the original keyboard interrupt handler did. Speaker changes are timestamped on the game thread and replayed on SDL's audio thread, so the sound keeps the game's timing.

## Regenerating the listing

`re/CAT.asm` was produced with Ghidra 12.1.4. To regenerate it, run this from the repository root:

```
mkdir -p /tmp/ghidra-proj
analyzeHeadless /tmp/ghidra-proj cat -import CAT.EXE -scriptPath re \
    -postScript DumpListing.java re/CAT.asm re/stats.txt -deleteProject
```

The script corrects the operand order of `MOV Sreg,r/m` instructions, which Ghidra prints reversed. It also marks the keyboard interrupt handlers and the room dispatch targets as code, because Ghidra does not find them on its own.

## License

[Unlicense](LICENSE)