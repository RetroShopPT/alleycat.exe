; data.asm - the original data segment (DS = para 0010h) of CAT.EXE.
;
; Bytes are taken verbatim from the EXE, so every DS offset in re/CAT.asm
; addresses the same byte here: [0690h] becomes [r12 + 0690h].

%define RUNTIME_DEFINES_DS
%include "runtime.inc"

section .data
align 16
global ds_base
ds_base:
    incbin "../CAT.EXE", 0x300, 0x7130      ; DS:0000..712F (file 0x300..0x742F)
    ; DS:7130 and up are not used by the game; the port's additions go here.
    times HELP_CTRL_L - ($ - ds_base) db 0
    db "   Ctrl-L  adds a life (up to 9).", 0
    times 0x10000 - ($ - ds_base) db 0      ; pad to a full 64 KB segment
