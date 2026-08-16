// Reports the retail startup gate used by Sudeki's DoOneLevelTest path.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TestArenaGateReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        println("");
        println("===== function=" + function.getEntryPoint() + " " +
            function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        Address gate = address(0x00808d88L);
        println("SudekiMP test-arena startup-gate report");
        println("SHA256=" + actualSha256);
        println("gate=" + gate);

        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(gate);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function function = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("reference=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " function=" +
                (function == null ? "<none>" : function.getEntryPoint() +
                    " " + function.getName(true)));
            decompile(decompiler, function, seen);
        }
        decompile(
            decompiler,
            currentProgram.getFunctionManager().getFunctionAt(address(0x005051f0L)),
            seen
        );
        decompiler.dispose();
    }
}
