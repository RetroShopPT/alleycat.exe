# Alley Cat (port)

[![version](https://img.shields.io/badge/release-1.0-orange)](https://github.com/petarov/alleycat.exe/releases/latest)
[![platforms-windows-mac-linux](https://img.shields.io/badge/platforms-windows,%20mac,%20linux-blue)](https://github.com/petarov/alleycat.exe/releases/latest)
[![license-unlicense](https://img.shields.io/badge/license-Unlicense-green)](https://github.com/petarov/alleycat.exe/blob/main/LICENSE)

A port of [Alley Cat](https://wikipedia.org/wiki/Alley_Cat_(video_game)) (Bill Williams, SynSoft/IBM, 1984) from its original DOS executable to `x86_64` platforms. The game was written in hand-coded 8088 assembly. This project disassembles `CAT.EXE`, translates the 16-bit code into x86-64 assembly, and runs it on emulated PC hardware with SDL3 handling video and sound.

> [!IMPORTANT]
> 99% LLM-generated code. I have rougly reviewed the output the agent produced, but if you have your concerns about AI, you should abstain from using this project.

![Alley Cat Title Screen](screenshots/title-screen.png)

## Controls

- Cursor keys move the cat. `Alt` performs special actions.
- In the options menu, `K`, `H`, `T` and `A` (or 1 to 4) choose the skill level. The skill sets the starting difficulty; the game raises it as you complete levels.
- `Ctrl-S`: turns the sound on and off.
- `Ctrl-R`: restarts the game.
- `Ctrl-M`: returns to the options menu.
- `Esc`: pauses the game whie playing; quits the game on the title screen.

### New (not in the original game):

 - `F10`, `Ctrl-W` and `Ctrl-Q`: quits at any time
 - `Ctrl-L`: adds a life, up to `9`

## Building

See [BUILDING.md](BUILDING.md) for requirements, build steps, the source layout and how the port works.

## License

[Unlicense](LICENSE)
