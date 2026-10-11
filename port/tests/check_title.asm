; check_title.asm - runs the generated backdrop code (CS:2A30) and the six
; title blits of title_screen (CS:5CC5..5D0A) with rand16 seeded to FA59h.
; tests/check_main.c dumps the resulting CGA memory for tools/ref_title.py.

%include "runtime.inc"

extern L_2A30
extern L_2D9D

global check_title

section .text

check_title:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
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

    mov word [r12 + 0x2ae5], 0xfa59
    mov word [r12 + 0x8], 0
    mov word [r12 + 0x6df8], 0
    call L_2A30

    ; title_screen CS:5CC5..5D0A (ES = B800h, set by CS:2A30)
    mov si, 0x6152
    mov cx, 0x1d0b
    mov di, 0x00bd
    call L_2D9D
    mov si, 0x63d0
    mov cx, 0x160e
    mov di, 0x069e
    call L_2D9D
    mov si, 0x6638
    mov cx, 0x0c03
    mov di, 0x0a78
    call L_2D9D
    mov si, 0x6680
    mov cx, 0x080e
    mov di, 0x0ca8
    call L_2D9D
    mov si, 0x6760
    mov cx, 0x0b0c
    mov di, 0x1d6e
    call L_2D9D
    mov si, 0x6868
    mov cx, 0x0804
    mov di, 0x1dec
    call L_2D9D

    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
