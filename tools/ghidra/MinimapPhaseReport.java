// Read-only exact-build report for UIMapManager update/snapshot/render
// ownership and submission timing.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class MinimapPhaseReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        println("");
        println("===== " + role + " target=" + address(value) + " function=" +
            (function == null ? "<none>" : function.getEntryPoint() + " " +
                function.getName(true)) + " =====");
        if (function == null) return;
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() : result.getErrorMessage());
        println("REFERENCES");
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        report(decompiler, 0x00487730L, "UIMapManager update");
        report(decompiler, 0x00487a10L, "UIMapManager world snapshot");
        report(decompiler, 0x00487a90L, "UIMapManager render submit");
        report(decompiler, 0x00487e10L, "last-cluster pointer transform");
        report(decompiler, 0x00435320L, "world snapshot caller");
        report(decompiler, 0x0049d8d0L, "in-game menu render dispatcher");
        report(decompiler, 0x0055f170L, "UI map poly render submit");
        report(decompiler, 0x0055f4b0L, "UI map poly transform commit");
        decompiler.dispose();
    }
}
