# Alley Cat (x86-64 port)

[![version](https://img.shields.io/badge/release-1.1-orange)](https://github.com/petarov/alleycat.exe/releases/latest)
[![platforms-windows-mac-linux](https://img.shields.io/badge/platforms-windows,%20mac,%20linux-blue)](https://github.com/petarov/alleycat.exe/releases/latest)
[![license-unlicense](https://img.shields.io/badge/license-Unlicense-green)](https://github.com/petarov/alleycat.exe/blob/main/LICENSE)

A port of [Alley Cat](https://wikipedia.org/wiki/Alley_Cat_(video_game)) (Bill Williams, SynSoft/IBM, 1984) from its original DOS executable to `x86_64` platforms.

> [!IMPORTANT]
> 99% LLM-generated code. I have rougly reviewed the output the agent produced, but if you have your concerns about AI, you should abstain from using this project.

![Alley Cat Title Screen](screenshots/title-screen.png)

## Platforms and downloads

| Platform | Latest release | Package to choose |
|---|---|---|
| Windows x64 | [1.1](https://github.com/petarov/alleycat.exe/releases/tag/1.1) | `windows-x86_64` ZIP |
| Linux x64 | [1.1](https://github.com/petarov/alleycat.exe/releases/tag/1.1) | `linux-x86_64` archive |
| macOS Intel (x64) | [1.1](https://github.com/petarov/alleycat.exe/releases/tag/1.1) | `macos-x86_64` ZIP |
| macOS Apple Silicon (ARM64) | [1.1](https://github.com/petarov/alleycat.exe/releases/tag/1.1) | `macos-x86_64` ZIP — runs under Rosetta 2 |

The macOS build is not signed or notarized: allow it under *Privacy & Security* or run `xattr -dr com.apple.quarantine <folder>`.

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
 - `Ctrl-K`: infinite lives (counter gets set to 0)

## Building

See [BUILDING.md](BUILDING.md) for requirements, build steps, the source layout and how the port works.

## License

[Unlicense](LICENSE)
