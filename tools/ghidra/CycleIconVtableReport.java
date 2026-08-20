// Exact-build, read-only UIElementCycleIcon vtable and lifecycle report.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class CycleIconVtableReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address a(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private long u32(long address) throws Exception {
        return Integer.toUnsignedLong(getInt(a(address)));
    }

    private void function(DecompInterface decompiler, long address,
                          String role) throws Exception {
        Function target = currentProgram.getFunctionManager().getFunctionAt(a(address));
        println("\n===== " + role + " " + a(address) + " =====");
        if (target == null) {
            println("missing=true");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(target, 240, monitor);
        println(result.decompileCompleted()
            ? result.getDecompiledFunction().getC() : result.getErrorMessage());

        println("CALLERS");
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        int count = 0;
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("callsite=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint()));
            if (++count == 48) {
                break;
            }
        }
        println("caller_count=" + count);
    }

    private void vtable(long address, int entries, String role) throws Exception {
        println("\n===== VTABLE " + role + " " + a(address) + " =====");
        for (int index = 0; index < entries; ++index) {
            long target = u32(address + index * 4L);
            println("slot=+0x" + Integer.toHexString(index * 4) +
                " target=" + a(target));
        }
    }

    @Override
    public void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new Exception("unexpected image");
        }
        vtable(0x006d8524L, 6, "UIElementCycleIcon primary");
        vtable(0x006d8540L, 2, "UIElementCycleIcon secondary");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] targets = {
            0x00480b00L, 0x005e8310L, 0x004a2900L, 0x0043b870L,
            0x0055c020L, 0x0055c230L, 0x0055c2c0L, 0x0055c270L,
            0x0055be70L, 0x0055c0e0L, 0x005b9ef0L, 0x004aa170L,
            0x004a9060L
        };
        String[] roles = {
            "primary_slot_00", "primary_slot_04", "primary_slot_08",
            "primary_slot_0c", "primary_slot_10", "primary_slot_14",
            "secondary_slot_00", "secondary_slot_04", "named_anchor_bind",
            "resource_assign", "ui_child_append", "portrait_gizmo_bind",
            "portrait_gizmo_construct"
        };
        for (int index = 0; index < targets.length; ++index) {
            function(decompiler, targets[index], roles[index]);
        }
        decompiler.dispose();
    }
}
