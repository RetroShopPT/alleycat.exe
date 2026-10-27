# Alley Cat — Apple Silicon (ARM64)

[![version](https://img.shields.io/badge/release-1.3.1--arm64-orange)](https://github.com/RetroShopPT/alleycat.exe/releases/latest)
[![platforms-windows-mac-linux](https://img.shields.io/badge/platforms-windows,%20mac,%20linux-blue)](https://github.com/petarov/alleycat.exe/releases/latest)
[![license-unlicense](https://img.shields.io/badge/license-Unlicense-green)](https://github.com/petarov/alleycat.exe/blob/main/LICENSE)

A recompilation of [Alley Cat](https://wikipedia.org/wiki/Alley_Cat_(video_game)) (Bill Williams, SynSoft/IBM, 1984) from its original DOS executable. This fork adds a **native macOS Apple Silicon (ARM64)** build using the portable C backend; the original x86-64 and web backends are retained.

Based on [petarov/alleycat.exe](https://github.com/petarov/alleycat.exe), version 1.3. Original game by **Bill Williams**, SynSoft/IBM (1984). Apple Silicon build integration and packaging are maintained in this fork.

> [!IMPORTANT]
> About 99% of the code is LLM-generated and only roughly reviewed. If you have concerns about AI-generated code, avoid this project.

| [CGA (default)](https://github.com/petarov/alleycat.exe/blob/main/screenshots/title-screen.png) | [PCjr](https://github.com/petarov/alleycat.exe/blob/main/screenshots/title-screen-pcjr.png) (`-pcjr` / Ctrl-P) |
|---|---|
| ![Title screen (CGA)](https://media.githubusercontent.com/media/petarov/alleycat.exe/refs/heads/main/screenshots/title-screen.png) | ![Title screen (PCjr colors)](https://media.githubusercontent.com/media/petarov/alleycat.exe/refs/heads/main/screenshots/title-screen-pcjr.png) |

## Platforms and downloads

| Platform | Latest release | Package to choose |
|---|---|---|
| [Web](https://petarov.github.io/alleycat.exe/) | [1.3](https://petarov.github.io/alleycat.exe/) | Play in your browser |
| Windows x64 | [1.3](https://github.com/petarov/alleycat.exe/releases/latest) | `windows-x86_64` ZIP |
| Linux x64 | [1.3](https://github.com/petarov/alleycat.exe/releases/latest) | `linux-x86_64` archive |
| macOS Intel (x64) | [1.3](https://github.com/petarov/alleycat.exe/releases/latest) | `macos-x86_64` ZIP |
| macOS Apple Silicon (ARM64) | [ARM64 releases](https://github.com/RetroShopPT/alleycat.exe/releases) | `alleycat-1.3.1-macos-arm64.zip` — native app, no Rosetta |

The macOS build is not signed or notarized: allow it under *Privacy & Security* or run `xattr -dr com.apple.quarantine <folder>`.

## Apple Silicon variant

The ARM64 app includes SDL2 and the game data needed at runtime. Unzip it and open **Alley Cat.app**. It runs directly on Apple Silicon, without Rosetta, DOSBox or a separate CAT.EXE beside the app. The release uses a local ad-hoc signature and is not Apple-notarized.

To build from source, supply your own supported `CAT.EXE`, install the universal SDL2 framework and follow [the Apple Silicon build instructions](BUILDING.md#native-apple-silicon-arm64). Generated listings, generated game C, game executables and app bundles are excluded from source control.

Validation on an M1 Pro with macOS 27.2: the title framebuffer matches the independent reference byte for byte; headless startup, skill selection, starting a game and cursor movement pass. This is a port of the existing runtime, with no claim of a complete gameplay or audio-fidelity validation. The build targets macOS 11 or newer; older macOS versions and an M4 have not been physically tested.

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
 - `Alt-Enter` or `F11` (`Cmd-Enter` or `Cmd-F11` on macOS): toggles fullscreen. The game starts in fullscreen by default. With `--window` on the command line, it starts in a window.

## Building

See [BUILDING.md](BUILDING.md) for requirements, build steps, the source layout and how the port works.

## License and attribution

This is a fork of petarov’s port; its history, credits and original LICENSE are preserved. The port source uses the Unlicense. **The original Alley Cat game and its data remain the property of their original rights holders; the port licence does not relicense those assets.** SDL2 retains its own licence, included in the bundled framework.


[Unlicense](LICENSE)
