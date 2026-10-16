# Alley Cat (x86-64 recomp)

[![version](https://img.shields.io/badge/release-1.2-orange)](https://github.com/petarov/alleycat.exe/releases/latest)
[![platforms-windows-mac-linux](https://img.shields.io/badge/platforms-windows,%20mac,%20linux-blue)](https://github.com/petarov/alleycat.exe/releases/latest)
[![license-unlicense](https://img.shields.io/badge/license-Unlicense-green)](https://github.com/petarov/alleycat.exe/blob/main/LICENSE)

A recompilation of [Alley Cat](https://wikipedia.org/wiki/Alley_Cat_(video_game)) (Bill Williams, SynSoft/IBM, 1984) from its original DOS executable for `x86_64` platforms.

> [!IMPORTANT]
> About 99% of the code is LLM-generated and only roughly reviewed. If you have concerns about AI-generated code, avoid this project.

| [CGA (default)](https://github.com/petarov/alleycat.exe/blob/main/screenshots/title-screen.png) | [PCjr](https://github.com/petarov/alleycat.exe/blob/main/screenshots/title-screen-pcjr.png) (`-pcjr` / Ctrl-P) |
|---|---|
| ![Title screen (CGA)](screenshots/title-screen.png) | ![Title screen (PCjr colors)](screenshots/title-screen-pcjr.png) |

## Platforms and downloads

| Platform | Latest release | Package to choose |
|---|---|---|
| Windows x64 | [1.2](https://github.com/petarov/alleycat.exe/releases/latest) | `windows-x86_64` ZIP |
| Linux x64 | [1.2](https://github.com/petarov/alleycat.exe/releases/latest) | `linux-x86_64` archive |
| macOS Intel (x64) | [1.2](https://github.com/petarov/alleycat.exe/releases/latest) | `macos-x86_64` ZIP |
| macOS Apple Silicon (ARM64) | [1.2](https://github.com/petarov/alleycat.exe/releases/latest) | `macos-x86_64` ZIP — runs under Rosetta 2 |

The macOS build is not signed or notarized: allow it under *Privacy & Security* or run `xattr -dr com.apple.quarantine <folder>`.

## Controls

- In the options menu, `K`, `H`, `T` and `A` (or 1 to 4) choose the skill level. The skill sets the starting difficulty; the game raises it as you complete levels.
- Use the cursor keys to move the cat and jump.
- `Alt` (PC) or `Option` (MacOS) performs special actions:
  - **Room 6** (milk bowls, sleeping dogs): hold it next to a bowl to drink from it. A full bowl takes several sips, and the room has 12 bowls.
  - **Room 7** (hearts cat room): drops a picked-up gift on the nearest shelf at the cat's position. A rival cat that walks into a gift takes it and stays away for a while. Each completed room adds one gift, up to 8.
- `Ctrl-S`: turns the sound on and off
- `Ctrl-R`: returns to the title screen
- `Esc`: pauses the game whie playing; quits the game on the title screen.

### New (not in the original game):

 - `F10`, `Ctrl-W` and `Ctrl-Q`: quits at any time
 - `Ctrl-L`: adds a life, up to `9`
 - `Ctrl-K`: infinite lives (counter gets set to 0)
 - `Ctrl-I`: invincibility against the dog, the cat head in the garbage cans and the rival cats in the hearts cat room; nothing is thrown from the windows
 - `Ctrl-P`: toggles the IBM PCjr colors and the CGA colors. With `-pcjr` on the command line, the game starts directly with the PCjr colors.
 - `Alt-Enter` or `F11` (`Cmd-Enter` or `Cmd-F11` on macOS): toggles fullscreen. With `--fullscreen` on the command line, the game starts in fullscreen. The mouse cursor is hidden in fullscreen.

## Building

See [BUILDING.md](BUILDING.md) for requirements, build steps, the source layout and how the port works.

## License

[Unlicense](LICENSE)
