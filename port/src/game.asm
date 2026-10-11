; game.asm - hand-written parts of the CAT.EXE port: runtime state, segment
; lookup, entry/exit, and the CS ranges tools/asm2nasm.py skips (HAND_WRITTEN).
; Everything else is generated into build/cat_code.asm from re/CAT.asm.

%define RUNTIME_DEFINES_STATE
%include "runtime.inc"

extern bda_seg
extern ivt_seg
extern rom_seg
extern game_bad_segment
extern L_0000

global seg_ds
global seg_es
global cs_seg
global ccall_ret
global seg_to_base
global game_main
global L_13A5
global L_1419
global game_in_title

section .bss
seg_ds:    resw 1
seg_es:    resw 1
ccall_ret: resd 1
exit_rsp:  resq 1
cs_seg:    resb 0x10000        ; code-segment variables (CS:13DF..13E7)
game_in_title: resb 1          ; 1 while title_screen (CS:5CB0) runs

section .text

; ---------------------------------------------------------------------------
; seg_to_base - in: r14 = 16-bit segment value; out: r14 = host base.
; Flags preserved. Segment values seen in the listing: 1010h (the data
; segment after relocation), B800h, 0040h, 0000h, F000h.

seg_to_base:
    pushfq
    cmp r14d, 0x1010
    je .data
    cmp r14d, 0xb800
    je .vram
    cmp r14d, 0x0040
    je .bda
    test r14d, r14d
    je .ivt
    cmp r14d, 0xf000
    je .rom
    mov edi, r14d
    and rsp, -16
    call game_bad_segment
.data:
    lea r14, [ds_base]
    popfq
    ret
.vram:
    lea r14, [cga_vram]
    popfq
    ret
.bda:
    lea r14, [bda_seg]
    popfq
    ret
.ivt:
    lea r14, [ivt_seg]
    popfq
    ret
.rom:
    lea r14, [rom_seg]
    popfq
    ret

; ---------------------------------------------------------------------------
; game_main - called from C on the game thread; runs main (CS:0000).
; DOS would enter with DS = ES = PSP; the data segment value stands in for it
; (main only pushes it for its final RETF, which game_exit replaces).

game_main:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov [exit_rsp], rsp
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    xor ebp, ebp
    mov word [seg_ds], 0x1010
    mov word [seg_es], 0x1010
    lea r12, [ds_base]
    mov r13, r12
    call L_0000
    ; main never returns with RET; fall through to the exit just in case.

game_exit:
    mov rsp, [exit_rsp]
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret

; ---------------------------------------------------------------------------
; CS:13A5 - exit to DOS (handle_hotkeys, Ctrl-Y): CALL restore_int_vectors,
; POP AX, RETF to PSP:0000. Here: leave game_main.

L_13A5:
    jmp game_exit

; ---------------------------------------------------------------------------
; CS:1419 install_int_vectors. The original saves INT 9 (and INT 48h on PCjr)
; and installs its keyboard handlers (CS:14B3 / 14FB / 1554). In the port the
; platform layer fills the key table, so there is nothing to install.

L_1419:
    ret
