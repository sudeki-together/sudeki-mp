// Reports the exact-build CDoor script activation boundary and callers.
// Read-only: no executable or project mutation.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class DoorActivationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler, Function function, String role) {
        if (function == null) {
            println("MISSING role=" + role);
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("\n===== " + role + " function=" +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
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
        Function door = currentProgram.getFunctionManager().getFunctionAt(
            address(0x004ce3a0L));
        decompile(decompiler, door, "CDoor::ActivateFromScript");
        decompile(
            decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                address(0x004ce300L)),
            "CDoor activation state transition helper");
        if (door != null) {
            ReferenceIterator references = currentProgram.getReferenceManager()
                .getReferencesTo(door.getEntryPoint());
            while (references.hasNext() && !monitor.isCancelled()) {
                Reference reference = references.next();
                if (!reference.getReferenceType().isCall()) {
                    continue;
                }
                Function caller = currentProgram.getFunctionManager()
                    .getFunctionContaining(reference.getFromAddress());
                println("CALLER callsite=" + reference.getFromAddress() +
                    " function=" + (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
                decompile(decompiler, caller, "CDoor activation caller");
            }
        }
        decompiler.dispose();
    }
}
