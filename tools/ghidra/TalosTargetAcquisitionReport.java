// Reports the ordinary CTargeter acquisition/validation path needed by the
// natural Talos-party experiment. Read-only and exact-build gated.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class TalosTargetAcquisitionReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void references(long value, String label) {
        ReferenceIterator iterator = currentProgram.getReferenceManager()
            .getReferencesTo(address(value));
        println("===== XREF " + label + " " + address(value) + " =====");
        while (iterator.hasNext()) {
            Reference reference = iterator.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("from=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" : caller.getName(true)));
        }
    }

    private void decompile(DecompInterface decompiler, long value, String label) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        println("===== " + label + " " + address(value) + " =====");
        if (function == null) {
            println("no function");
            return;
        }
        DecompileResults results = decompiler.decompileFunction(function, 90, monitor);
        if (!results.decompileCompleted()) {
            println("failed: " + results.getErrorMessage());
            return;
        }
        println(results.getDecompiledFunction().getC());
    }

    @Override
    protected void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(hash)) {
            throw new Exception("Unexpected executable SHA256: " + hash);
        }
        println("SudekiMP Talos ordinary-target acquisition report");
        println("SHA256=" + hash);
        long[] addresses = {
            0x00434c20L,
            0x004b7e10L,
            0x004b9410L,
            0x004b95b0L,
            0x004b9680L,
            0x004b9700L,
            0x004b9d20L,
            0x004b9ab0L,
            0x004ba1c0L,
            0x004ba6d0L
        };
        String[] labels = {
            "world candidate query",
            "target range/config resolver",
            "ordinary target geometric validator",
            "current target validator",
            "ordinary target clear/replace helper",
            "directional target search",
            "ordinary target commit",
            "CTargeter update",
            "ordinary target acquisition",
            "target selection/commit"
        };
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (int index = 0; index < addresses.length; ++index) {
            references(addresses[index], labels[index]);
            decompile(decompiler, addresses[index], labels[index]);
        }
        decompiler.dispose();
    }
}
