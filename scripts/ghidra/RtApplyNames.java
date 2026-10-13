// Road Trip: prepare the program and apply our names (run by scripts/ghidra/decompile.py).
//
// Arguments: <symbols file> [functions file]
//   symbols file:   lines "0xADDR<TAB>kind<TAB>name<TAB>note"; kind = fn | data
//   functions file: lines "0xADDR" of known function starts (from the recompiler's output)
//
// Adds the memory the ELF doesn't map (rest of RAM, scratchpad, GS registers), sets gp over the
// code, creates functions at known starts and applies names and plate comments. Idempotent.
//@category RoadTrip

import java.io.*;
import java.math.BigInteger;
import java.nio.file.*;
import java.util.*;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;

public class RtApplyNames extends GhidraScript {

    private static final long GP = 0x33AAF0L;

    private Address addr(long a) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(a);
    }

    private void ensureBlock(String name, long start, long size, boolean volatil) throws Exception {
        Memory mem = currentProgram.getMemory();
        Address s = addr(start);
        if (mem.getBlock(s) != null) return;
        MemoryBlock b = mem.createUninitializedBlock(name, s, size, false);
        b.setRead(true);
        b.setWrite(true);
        b.setVolatile(volatil);
    }

    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length < 1) throw new IllegalArgumentException("usage: RtApplyNames <symbols> [functions]");

        // Memory the ELF leaves out.
        Memory mem = currentProgram.getMemory();
        long ramEnd = 0;
        for (MemoryBlock b : mem.getBlocks()) {
            long end = b.getEnd().getOffset() + 1;
            if (end <= 0x2000000L && end > ramEnd) ramEnd = end;
        }
        if (ramEnd < 0x2000000L) ensureBlock("ram_tail", ramEnd, 0x2000000L - ramEnd, false);
        ensureBlock("ram_low", 0x80000L, 0x200000L - 0x80000L, false);
        ensureBlock("scratchpad", 0x70000000L, 0x4000L, false);
        // No blocks at the I/O registers (0x10000000..) or VU memory (0x11000000..): DMA tags and
        // VIF codes (0x1000000N CNT, 0x11000000 FLUSH) would print as pointers there.
        ensureBlock("gs_priv", 0x12000000L, 0x2000L, true);
        for (MemoryBlock b : mem.getBlocks()) {
            long s = b.getStart().getOffset();
            if (s >= 0x10000000L && s < 0x12000000L) {
                println("RtApplyNames: removing block " + b.getName() + " at " + b.getStart());
                mem.removeBlock(b, monitor);
            }
        }

        // The language's MMIO register names (as the add-on's ImportMMIORegisterLabels does).
        AddressSpace ram = currentProgram.getAddressFactory().getDefaultAddressSpace();
        SymbolTable st = currentProgram.getSymbolTable();
        for (Register r : currentProgram.getLanguage().getRegisters()) {
            if (r.getAddressSpace() != ram) continue;
            if (mem.getBlock(r.getAddress()) == null) continue;
            if (st.getGlobalSymbol(r.getName(), r.getAddress()) != null) continue;
            st.createLabel(r.getAddress(), r.getName(), SourceType.IMPORTED);
        }

        // gp is constant in this game.
        Register gp = currentProgram.getRegister("gp");
        ProgramContext ctx = currentProgram.getProgramContext();
        for (MemoryBlock b : mem.getBlocks()) {
            if (b.isExecute() && b.isInitialized()) {
                ctx.setValue(gp, b.getStart(), b.getEnd(), BigInteger.valueOf(GP));
            }
        }

        Listing listing = currentProgram.getListing();
        int made = 0;
        if (args.length > 1 && Files.exists(Paths.get(args[1]))) {
            for (String line : Files.readAllLines(Paths.get(args[1]))) {
                line = line.trim();
                if (line.isEmpty()) continue;
                Address a = addr(Long.decode(line));
                if (mem.getBlock(a) == null || !mem.getBlock(a).isExecute()) continue;
                if (listing.getFunctionAt(a) != null) continue;
                if (listing.getInstructionAt(a) == null) disassemble(a);
                if (createFunction(a, null) != null) made++;
            }
        }

        int named = 0;
        for (String line : Files.readAllLines(Paths.get(args[0]))) {
            if (line.isEmpty() || line.startsWith("#")) continue;
            String[] f = line.split("\t", -1);
            if (f.length < 3) continue;
            Address a = addr(Long.decode(f[0]));
            String kind = f[1], name = f[2], note = f.length > 3 ? f[3] : "";
            if (mem.getBlock(a) == null) continue;
            if (kind.equals("fn") && mem.getBlock(a).isExecute()) {
                Function fn = listing.getFunctionAt(a);
                if (fn == null) {
                    if (listing.getInstructionAt(a) == null) disassemble(a);
                    fn = createFunction(a, name);
                }
                if (fn != null) {
                    if (!fn.getName().equals(name)) fn.setName(name, SourceType.USER_DEFINED);
                } else {
                    st.createLabel(a, name, SourceType.USER_DEFINED).setPrimary();
                }
            } else {
                Symbol s = st.getGlobalSymbol(name, a);
                if (s == null) s = st.createLabel(a, name, SourceType.USER_DEFINED);
                s.setPrimary();
            }
            if (!note.isEmpty()) {
                listing.setComment(a, CommentType.PLATE, note);
            }
            named++;
        }
        println("RtApplyNames: " + made + " functions created, " + named + " names applied");
    }
}
