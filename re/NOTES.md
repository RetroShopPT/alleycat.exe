# CAT.EXE (Alley Cat, 1984): reverse-engineering notes

Target: `CAT.EXE`, 55,067 bytes, dated 1984-08-22, SHA-256 `4979c886…7bfdd`.
Tools: REA (Ghidra 12.1.4 provider) plus Ghidra headless with `DumpListing.java`.

**Legend:** **[O]** means observed directly in bytes or instructions. **[I]** means inferred from usage. The names are inferences. Nothing was executed.

## Verdict: hand-written 8088 assembly, not C

- [O] 263 procedures and **0** `PUSH BP / MOV BP,SP` frames.
- [O] Arguments are passed in registers (`blit_rect_to_cga` takes CL/CH/SI/DI). Booleans come back in CF (`rect_overlap` ends with `STC`/`CLC`).
- [O] There is **no `INT 21h` anywhere**. The program uses only BIOS (`INT 10h/11h/1Ah`) and direct hardware ports.
- [O] There is no CRT startup. The entry point does `PUSH DS; PUSH 0` and later leaves with `RETF` to PSP:0000 (`INT 20h`). That is the DOS 1.x exit style.
- [O] Mutable data lives inside the code segment: saved interrupt vectors at CS:13DF–13E6. There are no compiler or library strings.
- [O] At several points the code is padded with zeros up to a paragraph boundary (11F7→1200, 2E51→2E60, 5CA7→5CB0). [I] It was probably assembled from several MASM modules and linked with PARA alignment.

## Memory layout [O]

| Segment | Para | File offset | Size | Contents |
|---|---|---|---|---|
| Stack | 0000 | 0x200 | 0x100 | SS:SP = 0000:0100 |
| Data | 0010 | 0x300 | 0x7130 | Sprites (CGA 2bpp), tables, strings. File offset = DS offset + 0x300 |
| Code | 0723 | 0x7430 | 0x62EB | All code. Entry point is CS:0000 |

There are 9 relocations. Each one is the data-segment immediate (`MOV AX/DI,0010h`) loaded into DS or ES.

## Ghidra caveats

- [O] Ghidra 12.1.4 prints opcode `8E /r` with its operands reversed. For example, `8E C0` displays as `MOV AX,ES` but is really `MOV ES,AX`. `CAT.asm` corrects this. REA's live listings do not.
- Ghidra/REA addresses are *linear*, with load segment 0x1000. Code linear = 0x17230 + CS offset. Data linear = 0x10100 + DS offset. Ghidra's own data xrefs are unreliable here. Ghidra's own data xrefs were not validated. Use the DS+300h rule instead.
- REA's session reported 132 procedures. The headless pass in `DumpListing.java` reports 263, because default headless analysis finds more function starts and the script also seeds the jump table and the ISRs.
- Ghidra missed some code because it is reached only through the IVT. The script seeds it: the INT 9 ISRs at CS:14B3 (PC) and CS:14FB (PCjr), the PCjr INT 48h hook at CS:1554, and CS:13C7.

## Program structure

- **main (CS:0000)** [O]: check for CGA and set mode 4, then read the BIOS model byte at F000:FFFE (0xFD = PCjr). Install the INT 9 vector (plus INT 48h on PCjr) and run the title screen and options menu. The game loop follows.
- **Room dispatch** [O]: `JMP CS:[BX+0250h]` uses an 8-word table indexed by `[0004h]` (0–7). Cases 0 and 1 share a target. [I] The index picks the indoor room, and `rand16` chooses it at random. When `[0418h]` is set, case 7 is forced.
- [I] **Lives** are at `[1F80h]`. The value is set to 3 at game start, and the game returns to the menu when it reaches 0.

### Key subsystems

| CS | Name | Evidence |
|---|---|---|
| 5C60 | check_cga_set_mode4 | [O] Uses `INT 11h`, a B800 write test and mode 4. On failure it prints "This program requires a color/graphics adapter." and hangs |
| 13AA | get_bios_model_byte | [O] Reads F000:FFFE into `[0697h]` |
| 1419 / 147F | install / restore_int_vectors | [O] Saves the INT 9 (and PCjr INT 48h) vectors into CS:13DF and installs its own handlers |
| 14B3 / 14FB | int09_kbd_isr_pc / _pcjr | [O] Reads `IN AL,60h` and matches against 22 scancodes at DS:06A1 (`REPNE SCASB`). The state byte goes to DS:06B7+i, with bit 7 = released. Ends with an EOI to port 20h |
| 1200 | poll_input | [O] Reads the joystick through port 201h, timed with PIT channel 0 |
| 1338 | handle_hotkeys | [O] Handles the Ctrl hotkeys listed below |
| 5EE5 | options_menu | [O] Asks Y/N for joystick (`[069Bh]`), then K/H/T/A for skill level |
| 5E70 | paws_pause | [O] Esc pause. Saves screen to buffer and shows "Paws Game:" |
| 2D9D / 2DCA | blit_rect_to/from_cga | [O] `REP MOVSW` per row. Interlace step is `XOR DI,2000h` / `ADD DI,50h`. This is the most-called proc (70 callers) |
| 2CB0 | cga_xy_to_offset | [O] row×80 bytes (odd rows +1FD8h), x/4; pixel shift = (x&3)×2 |
| 2E29 | rect_overlap | [O] Bounding-box test with the result in CF. [I] Collision detection |
| 2DFD / 2E10 | rand16 / seed_rand_from_pit | [O] 16-bit LFSR at `[2AE5h]`, seeded from the PIT channel 0 latch |
| 5889 / 5B21 / 5B28 | speaker_on / off / beep | [O] PIT channel 2 (ports 42h/43h) and port 61h gate |
| 13D8 | cga_in_vretrace | [O] `IN 3DAh`, bit 3 |

### Keyboard table (DS:06A1 → state DS:06B7+i) [O]

`Alt ↑ → ↓ ← PgUp PgDn End Home Esc Y N K H T A S R Ctrl Del M 9`

### Hotkeys in handle_hotkeys

| Keys | Effect | Status |
|---|---|---|
| Ctrl-S | Toggles `[0000h]` and silences the speaker | [O] |
| Ctrl-R | Restart (`[041Bh]`) | [O] |
| Ctrl-M | Back to the menu (`[041Ch]`) | [O] |
| Esc | Calls `paws_pause` | [O] |
| **Ctrl-9** | Sets `[1F80h]` = 9. [I] Nine lives. Not on the help screen | Code [O], meaning [I] |
| **Ctrl-Y** | Jumps to CS:13A5: restores vectors, `POP AX`, `RETF` to the `PUSH DS/PUSH 0` from entry, which exits to DOS. All 8 call sites of `handle_hotkeys` are directly in `main`, so the stack depth works out. Scancode 15h is **Z on QWERTZ**. Not on the help screen | [O] |

These hotkeys were read from the code and have not been tested at runtime.

## Open items

- Most game procedures (rooms, enemies, scoring, music data around DS:54A5–5930) are still unnamed (`FUN_1723_xxxx`). `fingerprint.txt` tags each one by the hardware it touches and its caller count.
- The purpose of each of the 8 room cases has not been mapped.
- `13C7` is seeded as code but has no direct caller.

## Files

- `CAT.asm`: annotated listing of the code segment only. It is not reassemblable MASM, and the data segment is not emitted. It contains the Ghidra disassembly with names, call-target annotations and the `8E` fix, and shows data islands as `db`.
- `DumpListing.java`: GhidraScript that regenerates the listing:
  `analyzeHeadless <proj> cat -import CAT.EXE -scriptPath . -postScript DumpListing.java out.asm stats.txt -deleteProject`
- `stats.txt`: counts of prologues, INT and port use, and non-code runs.
- `fingerprint.txt`: per-procedure size, calls, callers and hardware tags.
