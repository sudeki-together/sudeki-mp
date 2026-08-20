// Exact-build, read-only report for the Load Game page's visual transition.
// @category SudekiMP
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class LoadGameVisualTransitionReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address a(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void dump(DecompInterface dc, long value, String role) {
        Function f = currentProgram.getFunctionManager().getFunctionAt(a(value));
        println("\n===== " + role + " address=" + a(value) + " =====");
        if (f == null) {
            println("missing=true");
            return;
        }
        DecompileResults result = dc.decompileFunction(f, 240, monitor);
        println("function=" + f.getName(true));
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() : result.getErrorMessage());
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(f.getEntryPoint());
        int count = 0;
        while (refs.hasNext() && count < 64 && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) continue;
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("callsite=" + ref.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint()));
            ++count;
        }
    }

    @Override
    public void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new Exception("unexpected image");
        }
        DecompInterface dc = new DecompInterface();
        dc.openProgram(currentProgram);
        long[] targets = {
            0x00520660L, 0x00520860L, 0x0051f9e0L, 0x0051fa60L,
            0x0051f7e0L, 0x00520260L, 0x00558ce0L, 0x004810a0L,
            0x00484130L, 0x00484260L
        };
        String[] roles = {
            "visual leave request", "visual enter request",
            "page transition notification", "page state apply",
            "base page activate", "resident UI state request",
            "resident UI update", "Load Game update",
            "Load Game enter", "Load Game leave"
        };
        for (int i = 0; i < targets.length; ++i) {
            dump(dc, targets[i], roles[i]);
        }
        dc.dispose();
    }
}
