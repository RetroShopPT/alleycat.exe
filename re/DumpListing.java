// Dumps the full CAT.EXE code segment as an assembly listing plus stats.
// args: <out_listing_path> <out_stats_path>
import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*;
import java.util.*;

public class DumpListing extends GhidraScript {
    static final long CODE_LIN = 0x17230L;   // linear start of code segment (para 0x723 + load seg 0x1000)
    static final long CODE_SEG_PARA = 0x723L;

    @Override
    public void run() throws Exception {
        String[] a = getScriptArgs();
        AddressSpace sp = currentProgram.getAddressFactory().getDefaultAddressSpace();
        Memory mem = currentProgram.getMemory();
        Listing lst = currentProgram.getListing();

        // Seed the room-dispatch jump table at CS:0250 (8 words).
        Address jt = sp.getAddress(CODE_LIN + 0x250);
        for (int i = 0; i < 8; i++) {
            int off = mem.getShort(jt.add(i * 2)) & 0xffff;
            Address tgt = sp.getAddress(CODE_LIN + off);
            new DisassembleCommand(tgt, null, true).applyTo(currentProgram, monitor);
            String old = lst.getComment(CodeUnit.PRE_COMMENT, tgt);
            String c = "room dispatch case " + i + " (CS:[BX+250h], [0004h]=" + i + ")";
            lst.setComment(tgt, CodeUnit.PRE_COMMENT, old == null ? c : old + "\n        ; " + c);
        }
        // Entry points reached only through the interrupt vector table or CS-relative code.
        long[][] seeds = { {0x14B3, 0}, {0x14FB, 1}, {0x1554, 2}, {0x13C7, 3} };
        String[] seedNames = { "int09_kbd_isr_pc", "int09_kbd_isr_pcjr", "int48_pcjr_kbd_xlat", "timer_delta_helper" };
        for (long[] sd : seeds) {
            Address t = sp.getAddress(CODE_LIN + sd[0]);
            new DisassembleCommand(t, null, true).applyTo(currentProgram, monitor);
            createFunction(t, seedNames[(int) sd[1]]);
        }
        analyzeChanges(currentProgram);

        // Names inferred from the code (hardware access, call sites, referenced strings).
        String[][] names = {
            {"0000", "main"}, {"04A0", "room_tick_04a0"}, {"08E5", "timed_update_08e5"},
            {"1200", "poll_input"}, {"1338", "handle_hotkeys"}, {"13AA", "get_bios_model_byte"},
            {"13B7", "read_pit_ch0"}, {"13D8", "cga_in_vretrace"}, {"13E8", "reset_key_state"},
            {"1419", "install_int_vectors"}, {"147F", "restore_int_vectors"},
            {"2CB0", "cga_xy_to_offset"}, {"2D9D", "blit_rect_to_cga"}, {"2DCA", "blit_rect_from_cga"},
            {"2DFD", "rand16"}, {"2E10", "seed_rand_from_pit"}, {"2E29", "rect_overlap"},
            {"5400", "draw_level_objects_5400"}, {"5889", "speaker_on"}, {"5B21", "speaker_off"}, {"5B28", "beep"},
            {"5C60", "check_cga_set_mode4"}, {"5C9E", "print_msg_set_ds"}, {"5CB0", "title_screen"},
            {"5E70", "paws_pause"}, {"5EE5", "options_menu"},
        };
        for (String[] n : names) {
            Function fn = getFunctionAt(sp.getAddress(CODE_LIN + Long.parseLong(n[0], 16)));
            if (fn == null) throw new IllegalStateException("no function at CS:" + n[0]);
            fn.setName(n[1], SourceType.USER_DEFINED);
        }

        MemoryBlock code = mem.getBlock(sp.getAddress(CODE_LIN));
        Address start = code.getStart(), end = code.getEnd();

        int prologues = 0, retf = 0, ret = 0, calls = 0;
        Map<String, Integer> ints = new TreeMap<>();
        Map<String, Integer> ports = new TreeMap<>();
        List<String> undef = new ArrayList<>();

        try (PrintWriter w = new PrintWriter(new FileWriter(a[0]))) {
            w.println("; CAT.EXE (Alley Cat, 1984) - Ghidra " + getGhidraVersion() + " listing of code segment 0723h");
            w.println("; addresses are CS:offset (CS = para 0723h); DS = para 0010h, SS = para 0000h SP=0100h");
            w.println("; data refs [xxxx] are DS-relative: file offset = xxxx + 300h");
            w.println();
            Address cur = start;
            Instruction prev = null;
            while (cur != null && cur.compareTo(end) <= 0) {
                Function f = getFunctionAt(cur);
                if (f != null) {
                    w.println();
                    w.println(";=========================================================");
                    w.printf("%s proc   ; CS:%04X  linear %s%n", f.getName(), cur.getOffset() - CODE_LIN, cur);
                    w.println(";=========================================================");
                }
                CodeUnit cu = lst.getCodeUnitAt(cur);
                if (cu instanceof Instruction) {
                    Instruction ins = (Instruction) cu;
                    String pre = ins.getComment(CodeUnit.PRE_COMMENT);
                    if (pre != null) w.println("        ; " + pre);
                    Symbol lbl = getSymbolAt(cur);
                    if (lbl != null && f == null && lbl.getSymbolType() == SymbolType.LABEL)
                        w.println(lbl.getName() + ":");
                    String tail = "";
                    for (Address ref : ins.getFlows()) {
                        Function tf = getFunctionAt(ref);
                        if (tf != null && ins.getFlowType().isCall())
                            tail = "   ; " + tf.getName();
                        else if (ref.getOffset() >= CODE_LIN)
                            tail = String.format("   ; CS:%04X", ref.getOffset() - CODE_LIN);
                    }
                    w.printf("%04X  %-14s %s%s%n", cur.getOffset() - CODE_LIN, hex(ins.getBytes()), fixText(ins), tail);
                    String m = ins.getMnemonicString();
                    if (m.equals("INT")) ints.merge(ins.getDefaultOperandRepresentation(0), 1, Integer::sum);
                    if (m.equals("IN") || m.equals("OUT")) ports.merge(m + " " + ins.toString().substring(m.length()).trim(), 1, Integer::sum);
                    if (m.equals("RETF")) retf++;
                    if (m.equals("RET")) ret++;
                    if (m.equals("CALL")) calls++;
                    if (prev != null && prev.toString().equals("PUSH BP") && ins.toString().equals("MOV BP,SP")) prologues++;
                    prev = ins;
                    cur = ins.getMaxAddress().next();
                } else {
                    // Gather a run of non-instruction bytes up to the next instruction or function.
                    Address runStart = cur;
                    StringBuilder sb = new StringBuilder();
                    int n = 0;
                    while (cur != null && cur.compareTo(end) <= 0 && !(lst.getCodeUnitAt(cur) instanceof Instruction)
                            && (n == 0 || getFunctionAt(cur) == null)) {
                        CodeUnit c2 = lst.getCodeUnitAt(cur);
                        int len = c2 == null ? 1 : c2.getLength();
                        for (int i = 0; i < len; i++) {
                            if (n % 16 == 0) {
                                if (n > 0) sb.append('\n');
                                sb.append(String.format("%04X  db ", cur.add(i).getOffset() - CODE_LIN));
                            } else sb.append(',');
                            sb.append(String.format("%02Xh", mem.getByte(cur.add(i)) & 0xff));
                            n++;
                        }
                        cur = cur.add(len);
                    }
                    undef.add(String.format("CS:%04X-%04X (%d bytes)", runStart.getOffset() - CODE_LIN,
                        runStart.getOffset() - CODE_LIN + n - 1, n));
                    w.println("        ; ---- " + n + " non-code bytes ----");
                    w.println(sb);
                    prev = null;
                }
            }
        }

        try (PrintWriter s = new PrintWriter(new FileWriter(a[1]))) {
            FunctionIterator fi = currentProgram.getFunctionManager().getFunctions(true);
            int nf = 0;
            while (fi.hasNext()) { fi.next(); nf++; }
            s.println("functions: " + nf);
            s.println("PUSH BP/MOV BP,SP prologues: " + prologues);
            s.println("RET: " + ret + "  RETF: " + retf + "  CALL: " + calls);
            s.println("INT usage: " + ints);
            s.println("IN/OUT usage: " + ports);
            s.println("non-code runs in code segment:");
            for (String u : undef) s.println("  " + u);
        }
    }

    // Ghidra 12.1.4 renders opcode 8E /r (MOV Sreg,r/m16) with operands reversed, e.g. 8E C0 as
    // "MOV AX,ES"; swap them back so the listing reads MOV ES,AX.
    private String fixText(Instruction ins) throws Exception {
        String t = ins.toString();
        byte[] b = ins.getBytes();
        int i = 0;
        while (i < b.length && (b[i] == 0x26 || b[i] == 0x2E || b[i] == 0x36 || b[i] == 0x3E)) i++;
        if (i < b.length && (b[i] & 0xff) == 0x8E && t.startsWith("MOV ")) {
            String ops = t.substring(4);
            int comma = ops.indexOf(',');
            String dst = ops.substring(0, comma), src = ops.substring(comma + 1);
            if (dst.matches("[A-D]X|SI|DI|BP|SP") || dst.contains("[")) {
                if (src.matches("[CDES]S")) t = "MOV " + src + "," + dst;
            }
        }
        return t;
    }

    private static String hex(byte[] b) {
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02X", x & 0xff));
        return sb.toString();
    }
}
