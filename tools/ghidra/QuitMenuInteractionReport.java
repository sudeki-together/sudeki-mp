// Reports the exact native Quit-menu row, navigation, selection, and render paths.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class QuitMenuInteractionReport extends GhidraScript {
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
        println("===== " + role + " address=" + address(value) + " =====");
        if (function == null) {
            println("No function");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
        println("CALLERS");
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint()));
        }
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP Quit-menu interaction report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] targets = {
            0x0041cfe0L,
            0x0041d120L,
            0x0041d780L,
            0x0041d860L,
            0x0041d8b0L,
            0x0041d930L,
            0x0041d9f0L,
            0x0041db00L
        };
        String[] roles = {
            "Quit native option/header render",
            "Quit native text/action render",
            "Quit confirm/select dispatch",
            "Quit cancel/back dispatch",
            "Quit open eligibility",
            "Quit analog navigation",
            "Quit digital navigation",
            "Quit input event dispatcher"
        };
        for (int index = 0; index < targets.length; ++index) {
            report(decompiler, targets[index], roles[index]);
        }
        decompiler.dispose();
    }
}
