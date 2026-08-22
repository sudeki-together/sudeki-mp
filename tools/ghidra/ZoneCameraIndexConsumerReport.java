// Read-only exact-build report for consumers of the temporary-world camera index.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ZoneCameraIndexConsumerReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address a(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void dump(DecompInterface d, long va, String label) {
        Function f = currentProgram.getFunctionManager().getFunctionContaining(a(va));
        println("\n===== " + label + " @ " + a(va) + " =====");
        if (f == null) {
            println("<missing function>");
            return;
        }
        println("function=" + f.getName(true) + " entry=" + f.getEntryPoint());
        DecompileResults r = d.decompileFunction(f, 180, monitor);
        if (r.decompileCompleted() && r.getDecompiledFunction() != null) {
            println(r.getDecompiledFunction().getC());
        } else {
            println("<decompile failed>");
        }
    }

    @Override
    protected void run() throws Exception {
        if (!SHA.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable SHA256");
        }
        DecompInterface d = new DecompInterface();
        d.openProgram(currentProgram);
        long[] hits = {
            0x0043513bL, 0x004cf655L,
            0x004066afL, 0x0040689dL, 0x00435305L,
            0x004eaf2aL, 0x004ee9dcL, 0x004ef916L,
            0x004f12c3L, 0x004f1c29L, 0x004f1c60L,
            0x004f1ce3L, 0x004f1d72L, 0x005472a8L,
            0x005ad07aL, 0x005b6f26L
        };
        for (long hit : hits) {
            dump(d, hit, "camera-index field read");
        }
        d.dispose();
    }
}
