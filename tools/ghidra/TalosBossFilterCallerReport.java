// Reports the exact-build callers around the AI candidate boss-type filter.
// Read-only; used to identify ownership and construction of query byte +0x25.
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

public class TalosBossFilterCallerReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";
    private static final long[] TARGETS = {
        0x005b6ec0L,
        0x005b7120L
    };

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
        DecompInterface decompiler,
        Function function,
        Set<Address> seen,
        String relationship
    ) {
        DecompileResults results;
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        println("\n===== " + relationship + " " +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        results = decompiler.decompileFunction(function, 120, monitor);
        println(results.decompileCompleted() ?
            results.getDecompiledFunction().getC() :
            results.getErrorMessage());
    }

    private void reportCallers(
        DecompInterface decompiler,
        Address target,
        Set<Address> seen,
        int depth
    ) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller;
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("call depth=" + depth + " from=" +
                reference.getFromAddress() + " to=" + target +
                " caller=" + (caller == null ? "<none>" :
                    caller.getEntryPoint().toString()));
            decompile(decompiler, caller, seen, "CALLER_DEPTH_" + depth);
            if (caller != null && depth < 3) {
                reportCallers(decompiler, caller.getEntryPoint(), seen,
                    depth + 1);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        DecompInterface decompiler;
        Set<Address> seen;
        if (!EXPECTED_SHA256.equalsIgnoreCase(
                currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        seen = new HashSet<Address>();
        for (long value : TARGETS) {
            Address target = address(value);
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(target);
            decompile(decompiler, function, seen, "TARGET");
            reportCallers(decompiler, target, seen, 1);
        }
        println("FUNCTION_COUNT=" + seen.size());
        decompiler.dispose();
    }
}
