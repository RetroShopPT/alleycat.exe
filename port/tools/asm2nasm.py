#!/usr/bin/env python3
"""Translate the Ghidra listing of CAT.EXE (re/CAT.asm) into x86-64 nasm.

Usage: asm2nasm.py <re/CAT.asm> <out.asm>

Every instruction gets a label L_xxxx (its CS offset), so branches, shared
tails and fallthrough between procedures keep working. The register and
segment model is described in src/runtime.inc. CS ranges listed in
HAND_WRITTEN are skipped; src/game.asm supplies their labels. Instructions
the translator does not handle become TRAP <cs>, which aborts at runtime with
the offset, so the output always assembles.
"""
import re
import sys

LINEAR_TO_CS = 0x7230  # "0x1000:xxxx" in the listing is CS:(xxxx - 7230h)

# [start, end] inclusive CS ranges implemented by hand in src/game.asm.
HAND_WRITTEN = [
    (0x13A5, 0x13A9),  # exit to DOS: restore vectors, POP AX, RETF
    (0x1419, 0x15CF),  # install/restore vectors, INT 9 / INT 48h handlers
]

# CS offset -> nanoseconds of 8088 time to spend before that instruction.
# Only for loops whose sound depends on CPU speed: they toggle the speaker
# from software, once per pass. Values are estimated 8088 cycle counts of one
# pass at 4.77 MHz.
CPU_PACE_NS = {
    0x57A6: 75000,  # PWM tone, called once per vretrace poll (loop at CS:3544)
    0x59EB: 75000,  # noise burst inner loop around INT 1Ah (FUN_59CB)
}

# CS offset -> lines emitted before that instruction: markers that tell the
# platform layer where the game is, and the port's changes to the game.
MARKERS = {
    # title_screen entry / main after CALL title_screen. Flags not affected.
    0x5CB0: ['mov byte [game_in_title], 1'],
    0x0099: ['mov byte [game_in_title], 0'],
    # handle_hotkeys entry: the port's Ctrl-L (bios.c). Registers and flags kept.
    0x1338: ['CCALL port_hotkeys'],
    # Loops that only wait for a key (keypress counter [0693h] to change):
    # sleep 1 ms per pass instead of spinning a host core. Registers and flags
    # kept. Joystick question, skill menu, "Press any key" / Esc pause, text
    # screen at CS:5FF6.
    0x5EF7: ['CCALL port_idle'],
    0x5F29: ['CCALL port_idle'],
    0x5FAA: ['CCALL port_idle'],
    0x6007: ['CCALL port_idle'],
    # The port's Ctrl-K infinite lives keeps [1F80h] at 0 (bios.c), so the
    # "no lives left" checks take their "lives left" path while it is on.
    # The flags at each target match the original path (ZF=0, CF=0).
    0x00F3: ['cmp byte [infinite_lives], 0', 'jne L_00FA'],
    0x0155: ['cmp byte [infinite_lives], 0', 'jne L_015F'],
    0x01AD: ['cmp byte [infinite_lives], 0', 'jne L_01B7'],
    0x1DB8: ['cmp byte [infinite_lives], 0', 'jne L_1DBF'],
    # The port's Ctrl-I invincibility (bios.c). The dog's catch test (CS:20F5)
    # takes its "not caught" exit (CLC; RET). In the female cat level
    # (room 7), CS:4DD0 tests one of 7 cats against the player: slot 6 is the
    # female cat (win), slots 0-5 knock the player down at CS:4E00. That path
    # instead returns "no overlap" through rect_overlap's CLC; RET (CS:2E4F).
    0x20F5: ['cmp byte [invincible], 0', 'jne L_2134'],
    0x4E00: ['cmp byte [invincible], 0', 'jne L_2E4F'],
    # The cat head popping out of a garbage can: CS:22F7 tests it against the
    # player (the trip off the can at CS:0E4A, and [1D58h] calls the dog).
    # Its overlap path also returns "no overlap" through CS:2E4F.
    0x2300: ['cmp byte [invincible], 0', 'jne L_2E4F'],
    # Objects thrown from the windows: CS:1A1A starts a throw. While Ctrl-I is
    # on it takes the "no throw" exit (CS:1A75); the caller ignores flags.
    0x1A1A: ['cmp byte [invincible], 0', 'jne L_1A75'],
    # options_menu: skip the joystick question and take its "N" path (keyboard
    # selected, next text line is index 1, the skill prompt).
    0x5EEB: ['mov word [r12 + 0x6d8f], 0x2',
             'mov byte [r12 + 0x69b], 0x0',
             'jmp L_5F1C'],
    # options_menu, after the "During play:" help lines (text rows 9-13): add
    # the Ctrl-L, Ctrl-I, Ctrl-P and fullscreen lines on rows 14-17 (strings
    # in data.asm). The keyboard texts printed next ("Use the cursor keys...",
    # "The Alt key...") move from rows 15-16 to 19-20 and "Press any key to
    # start." from row 20 to 22: their row table entries (DS:6D63 + 1Ch/1Eh/16h,
    # row*256 + column) are rewritten. DX, SI and the registers
    # L_5E5B/L_5E2B change are reloaded by the code that follows.
    0x5F5D: ['mov dx, 0x0e00',
             'call L_5E5B',
             'mov si, HELP_CTRL_L',
             'call L_5E2B',
             'mov dx, 0x0f00',
             'call L_5E5B',
             'mov si, HELP_CTRL_I',
             'call L_5E2B',
             'mov dx, 0x1000',
             'call L_5E5B',
             'mov si, HELP_CTRL_P',
             'call L_5E2B',
             'mov dx, 0x1100',
             'call L_5E5B',
             'mov si, HELP_FULLSCREEN',
             'call L_5E2B',
             'mov word [r12 + 0x6d7f], 0x1300',
             'mov word [r12 + 0x6d81], 0x1400',
             'mov word [r12 + 0x6d79], 0x1600'],
}

REG16 = {'AX': 'ax', 'BX': 'bx', 'CX': 'cx', 'DX': 'dx', 'SI': 'si', 'DI': 'di', 'BP': 'bp', 'SP': 'sp'}
REG8 = {'AL': 'al', 'BL': 'bl', 'CL': 'cl', 'DL': 'dl', 'AH': 'ah', 'BH': 'bh', 'CH': 'ch', 'DH': 'dh'}
HIGH_TO_LOW = {'ah': 'al', 'bh': 'bl', 'ch': 'cl', 'dh': 'dl'}
REG64 = {'AX': 'rax', 'BX': 'rbx', 'CX': 'rcx', 'DX': 'rdx', 'SI': 'rsi', 'DI': 'rdi', 'BP': 'rbp'}
SEGS = ('ES', 'DS', 'CS', 'SS')
SEG_BASE = {'DS': 'r12', 'ES': 'r13'}

# The only indirect jump: JMP word ptr CS:[BX + 0x250] in main. BX is the case
# index times 2 (main clamps it to 0..7); the 8 code offsets are read from the
# listing's db row at the table address.
JUMP_TABLE_RE = re.compile(r'word ptr CS:\[BX \+ 0x([0-9a-f]+)\]')
JUMP_TABLE_ENTRIES = 8

# Mnemonics that translate 1:1 once operands are rewritten.
SIMPLE = {
    'MOV', 'ADD', 'SUB', 'CMP', 'AND', 'OR', 'XOR', 'TEST', 'ADC', 'SBB', 'INC', 'DEC',
    'NOT', 'NEG', 'SHL', 'SHR', 'SAR', 'ROL', 'ROR', 'RCL', 'RCR', 'MUL', 'XCHG',
}
JUMPS = {
    'JMP', 'JZ', 'JNZ', 'JC', 'JNC', 'JA', 'JBE', 'JS', 'JNS', 'JL', 'JGE', 'JLE', 'JG',
    'JO', 'JNO', 'JP', 'JNP',
}
NOARG = {'CLD': 'cld', 'STD': 'std', 'CLC': 'clc', 'STC': 'stc', 'CMC': 'cmc', 'NOP': 'nop',
         'RET': 'ret', 'PUSHF': 'pushfq', 'POPF': 'popfq', 'CLI': 'nop', 'STI': 'nop',
         'LAHF': 'LAHF16', 'SAHF': 'SAHF16', 'AAA': 'AAA16'}
STRING_RE = re.compile(r'(MOVS|STOS|LODS|SCAS|CMPS)([BW])(?:\.(REP|REPNE|REPE))?$')

INSN_RE = re.compile(r'([0-9A-F]{4})  ([0-9A-F]+) +(\S+)\s*(.*)$')
PROC_RE = re.compile(r'(\S+) proc   ; CS:([0-9A-F]{4})')


class Untranslatable(Exception):
    pass


def target_label(text):
    m = re.fullmatch(r'0x1000:([0-9a-f]+)', text)
    if not m:
        raise Untranslatable('branch target ' + text)
    return 'L_%04X' % (int(m.group(1), 16) - LINEAR_TO_CS)


def split_operands(text):
    """Split on commas outside brackets."""
    ops, depth, cur = [], 0, ''
    for ch in text:
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
        if ch == ',' and depth == 0:
            ops.append(cur.strip())
            cur = ''
        else:
            cur += ch
    if cur.strip():
        ops.append(cur.strip())
    return ops


class Operand:
    """One translated operand plus the setup lines it needs (address LEA)."""

    def __init__(self, text):
        self.setup = []
        self.is_mem = False
        self.high = None
        t = text.strip()
        size = ''
        m = re.match(r'(byte|word) ptr (.*)$', t)
        if m:
            size, t = m.group(1) + ' ', m.group(2)
        seg, explicit = 'DS', False
        m = re.match(r'(ES|DS|CS|SS):(\[.*\])$', t)
        if m:
            seg, t, explicit = m.group(1), m.group(2), True
        if t.startswith('['):
            self.is_mem = True
            self.text = self._memory(t[1:-1], seg, explicit, size)
        elif t in REG16:
            self.text = REG16[t]
        elif t in REG8:
            self.text = REG8[t]
            if self.text in HIGH_TO_LOW:
                self.high = self.text
        elif re.fullmatch(r'0x[0-9a-f]+', t):
            self.text = t
        else:
            raise Untranslatable('operand ' + text)

    def _memory(self, inner, seg, explicit, size):
        terms = [x.strip() for x in inner.split('+')]
        regs, disp = [], 0
        for term in terms:
            if term in ('BX', 'SI', 'DI', 'BP'):
                regs.append(term)
            elif re.fullmatch(r'0x[0-9a-f]+', term):
                disp += int(term, 16)
            else:
                raise Untranslatable('address ' + inner)
        if 'BP' in regs and not explicit:
            raise Untranslatable('BP addressing defaults to SS')
        if seg == 'CS' and not regs:
            # Code-segment variables (CS:13DF..13E7) live in cs_seg.
            return '%s[cs_seg + 0x%x]' % (size, disp & 0xFFFF)
        if seg not in SEG_BASE:
            raise Untranslatable(seg + ' segment operand')
        base = SEG_BASE[seg]
        if not regs:
            return '%s[%s + 0x%x]' % (size, base, disp & 0xFFFF)
        # Exact 16-bit effective address, flag-free: lea + movzx into r14.
        expr = ' + '.join(REG64[r] for r in regs)
        self.setup = ['lea r14d, [%s + 0x%x]' % (expr, disp & 0xFFFF), 'movzx r14d, r14w']
        return '%s[%s + r14]' % (size, base)


def translate(mnem, args, jump_tables):
    """Return a list of nasm lines for one listing instruction."""
    args = args.split('   ;')[0].strip()

    m = JUMP_TABLE_RE.fullmatch(args)
    if mnem == 'JMP' and m:
        table = int(m.group(1), 16)
        if table not in jump_tables:
            raise Untranslatable('jump table at CS:%04X without data' % table)
        # Table of 32-bit offsets relative to the table; BX = case*2 -> rbx*2.
        return ['lea r14, [jt_%04X]' % table, 'movsxd r15, dword [r14 + rbx*2]',
                'add r15, r14', 'jmp r15']

    if mnem in JUMPS:
        return ['%s %s' % (mnem.lower(), target_label(args))]
    if mnem == 'CALL':
        return ['call ' + target_label(args)]
    if mnem in ('LOOP', 'LOOPZ', 'LOOPNZ'):
        return ['%s16 %s' % (mnem, target_label(args))]
    if mnem in NOARG and not args:
        return [NOARG[mnem]]

    m = STRING_RE.match(mnem)
    if m:
        op, width, rep = m.groups()
        insn = op.lower() + width.lower()
        if rep:
            insn = rep.lower() + ' ' + insn
        return ['STRINGOP {%s}' % insn]

    if mnem == 'INT':
        n = int(args, 16)
        if n in (0x10, 0x11, 0x1A):
            return ['BIOS bios_int%02x' % n]
        raise Untranslatable('INT ' + args)
    if mnem == 'IN':
        dst, port = split_operands(args)
        if dst != 'AL':
            raise Untranslatable('IN ' + args)
        return ['PORT_IN_DX' if port == 'DX' else 'PORT_IN %s' % port]
    if mnem == 'OUT':
        port, src = split_operands(args)
        if src != 'AL':
            raise Untranslatable('OUT ' + args)
        return ['PORT_OUT_DX' if port == 'DX' else 'PORT_OUT %s' % port]

    if mnem in ('PUSH', 'POP'):
        if args in REG64:
            return ['%s %s' % (mnem.lower(), REG64[args])]
        if args in ('DS', 'ES'):
            return ['%s_SEG %s' % (mnem, args.lower())]
        raise Untranslatable(mnem + ' ' + args)

    ops = split_operands(args)
    if mnem == 'MOV' and len(ops) == 2 and ops[0] in ('DS', 'ES'):
        if ops[1] not in REG16:
            raise Untranslatable('MOV sreg from ' + ops[1])
        return ['SET_SEG %s, %s' % (ops[0].lower(), REG16[ops[1]])]
    if mnem == 'MOV' and len(ops) == 2 and ops[1] in SEGS:
        if ops[0] in REG16 and ops[1] in ('DS', 'ES'):
            return ['GET_SEG %s, %s' % (REG16[ops[0]], ops[1].lower())]
        raise Untranslatable('MOV from ' + ops[1])

    if mnem not in SIMPLE:
        raise Untranslatable('mnemonic ' + mnem)

    parsed = [Operand(o) for o in ops]
    setup = [line for p in parsed for line in p.setup]
    text = '%s %s' % (mnem.lower(), ', '.join(p.text for p in parsed))
    has_mem = any(p.is_mem for p in parsed)
    highs = [p.high for p in parsed if p.high]
    if has_mem and highs:
        if len(highs) > 1:
            raise Untranslatable('two high-byte registers with memory')
        high = highs[0]
        low = HIGH_TO_LOW[high]
        if re.search(r'\b%s\b' % low, text):
            raise Untranslatable('high and low byte of the same register with memory')
        text = re.sub(r'\b%s\b' % high, low, text)
        return setup + ['VIA_LOW %s, %s, {%s}' % (high, low, text)]
    return setup + [text]


def in_hand_written(cs):
    return any(a <= cs <= b for a, b in HAND_WRITTEN)


def main():
    src, out = sys.argv[1], sys.argv[2]
    rows = []           # (cs, raw bytes, mnemonic, operands) in listing order
    proc_names = {}
    db_bytes = {}       # CS offset -> byte value, from data rows
    for line in open(src, encoding='ascii'):
        d = re.match(r'([0-9A-F]{4})  db (.*)$', line.strip())
        if d:
            base = int(d.group(1), 16)
            for i, v in enumerate(d.group(2).split(',')):
                db_bytes[base + i] = int(v.rstrip('h'), 16)
            continue
        m = PROC_RE.match(line)
        if m:
            proc_names[int(m.group(2), 16)] = m.group(1)
            continue
        m = INSN_RE.match(line.rstrip('\n'))
        if m and not m.group(3) == 'db':
            rows.append((int(m.group(1), 16), m.group(2), m.group(3), m.group(4)))

    jump_tables = {}
    for cs, raw, mnem, args in rows:
        m = JUMP_TABLE_RE.fullmatch(args.split('   ;')[0].strip())
        if mnem == 'JMP' and m:
            t = int(m.group(1), 16)
            entries = [db_bytes[t + 2 * i] | db_bytes[t + 2 * i + 1] << 8 for i in range(JUMP_TABLE_ENTRIES)]
            jump_tables[t] = entries

    referenced = set()
    body = []
    traps = 0
    for cs, raw, mnem, args in rows:
        if in_hand_written(cs):
            continue
        if cs in proc_names:
            body.append('')
            body.append('; ---- %s (CS:%04X)' % (proc_names[cs], cs))
        body.append('L_%04X:' % cs)
        for ln in MARKERS.get(cs, []):
            body.append('    ' + ln)
        if cs in CPU_PACE_NS:
            body.append('    CPU_PACE %d' % CPU_PACE_NS[cs])
        try:
            lines = translate(mnem, args, jump_tables)
        except Untranslatable as e:
            lines = ['TRAP 0x%04X   ; %s %s (%s)' % (cs, mnem, args.split('   ;')[0], e)]
            traps += 1
        for ln in lines:
            for lab in re.findall(r'L_([0-9A-F]{4})', ln):
                referenced.add(int(lab, 16))
            body.append('    ' + ln)

    defined = {cs for cs, *_ in rows if not in_hand_written(cs)}
    externs = sorted(x for x in referenced if x not in defined)
    exported = sorted(set(proc_names) & defined)

    with open(out, 'w', encoding='ascii') as f:
        f.write('; GENERATED by tools/asm2nasm.py from re/CAT.asm - do not edit.\n')
        f.write('; %d instructions, %d TRAPs.\n\n' % (len(defined), traps))
        f.write('%include "runtime.inc"\n\n')
        for x in externs:
            f.write('extern L_%04X\n' % x)
        for x in exported:
            f.write('global L_%04X\n' % x)
        f.write('\nsection .text\n')
        f.write('\n'.join(body))
        f.write('\n')
        for t, entries in sorted(jump_tables.items()):
            f.write('\nalign 4\njt_%04X:\n' % t)
            for e in entries:
                f.write('    dd L_%04X - jt_%04X\n' % (e, t))
    print('%s: %d instructions, %d procedures exported, %d externs, %d TRAPs'
          % (out, len(defined), len(exported), len(externs), traps))


if __name__ == '__main__':
    main()
