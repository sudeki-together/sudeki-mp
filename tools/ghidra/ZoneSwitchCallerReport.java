// Enumerate static callers of the exact-build zone transition exports.
// Read-only: no executable or project mutation.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ZoneSwitchCallerReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void reportCallers(DecompInterface decompiler, long rva,
            String role) {
        Function target = currentProgram.getFunctionManager()
            .getFunctionAt(address(0x00400000L + rva));
        println("\n===== " + role + " target=" +
            (target == null ? "<missing>" : target.getEntryPoint() + " " +
            target.getName(true)) + " =====");
        if (target == null) return;
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("CALLER callsite=" + reference.getFromAddress() +
                " function=" + (caller == null ? "<none>" :
                caller.getEntryPoint() + " " + caller.getName(true)));
            if (caller != null) {
                DecompileResults result = decompiler.decompileFunction(
                    caller, 90, monitor);
                if (result.decompileCompleted()) {
                    println(result.getDecompiledFunction().getC());
                }
            }
        }
    }

    @Override
    protected void run() throws Exception {
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256: " +
                currentProgram.getExecutableSHA256());
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        reportCallers(decompiler, 0x00007990L, "SwitchZoneNOW callers");
        reportCallers(decompiler, 0x00007970L, "EnterZone callers");
        reportCallers(decompiler, 0x000064b0L,
            "CWorld::EnterTemporaryZone callers");
        reportCallers(decompiler, 0x00006710L,
            "CWorld::ExitTemporaryZone callers");
        decompiler.dispose();
    }
}
