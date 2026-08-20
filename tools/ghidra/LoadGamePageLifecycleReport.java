// Reports the exact Load Game page and title-controller lifecycle used to
// construct the four native character portrait widgets.
// Read-only and hash-gated to the supported GOG executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class LoadGamePageLifecycleReport extends GhidraScript {
    private static final String SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long LOAD_GAME_VTABLE = 0x006ca89cL;
    private static final long TITLE_CONTROLLER_VTABLE = 0x006cb1fcL;

    private Address at(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private long pointer(Memory memory, long value) throws Exception {
        return Integer.toUnsignedLong(memory.getInt(at(value)));
    }

    private void function(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(at(value));
        println("\n===== " + role + " address=" + at(value) + " =====");
        if (function == null) {
            println("missing=true");
            return;
        }
        println("function=" + function.getEntryPoint() + " " +
            function.getName(true));
        if (!seen.add(function.getEntryPoint())) {
            println("already_decompiled=true");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 180,
            monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "decompile_error=" + result.getErrorMessage());
    }

    private void vtable(DecompInterface decompiler, Memory memory,
            long table, int bytes, String role, Set<Address> seen)
            throws Exception {
        println("\n===== " + role + " vtable=" + at(table) + " =====");
        for (int offset = 0; offset < bytes; offset += 4) {
            long target = pointer(memory, table + offset);
            println("slot=0x" + Integer.toHexString(offset) +
                " target=" + at(target));
            function(decompiler, target,
                role + " vtable+0x" + Integer.toHexString(offset), seen);
        }
    }

    private void callers(long value, String role) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(at(value));
        int count = 0;
        println("\nCALLERS role=" + role + " target=" + at(value));
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("callsite=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            if (++count >= 80) {
                println("truncated=true");
                break;
            }
        }
        println("count=" + count);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP Load Game page lifecycle report");
        println("SHA256=" + actual);
        Memory memory = currentProgram.getMemory();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        vtable(decompiler, memory, LOAD_GAME_VTABLE, 0x70,
            "Load Game page", seen);
        vtable(decompiler, memory, TITLE_CONTROLLER_VTABLE, 0x60,
            "title controller", seen);

        long[] boundaries = {
            0x004a0f40L, 0x004a2ca0L, 0x004898a0L, 0x0048d970L,
            0x0048a590L, 0x0048a900L, 0x0048c710L, 0x0055be70L,
            0x0055c070L, 0x0055c230L
        };
        String[] roles = {
            "title state transition", "PC renderer-controller update",
            "save page action", "save page UI input",
            "save page UI entry", "save page input",
            "save entry update", "cycle icon bind",
            "portrait resource selector", "cycle icon refresh"
        };
        for (int index = 0; index < boundaries.length; ++index) {
            function(decompiler, boundaries[index], roles[index], seen);
            callers(boundaries[index], roles[index]);
        }
        decompiler.dispose();
    }
}
