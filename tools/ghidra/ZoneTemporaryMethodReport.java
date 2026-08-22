// Read-only exact-build report for the temporary-zone method and nearby
// world-transition helpers.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class ZoneTemporaryMethodReport extends GhidraScript {
    private static final String SHA =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address a(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void dump(DecompInterface d, long va, String label) {
        Function f = currentProgram.getFunctionManager().getFunctionAt(a(va));
        println("\n===== " + label + " @ " + a(va) + " =====");
        if (f == null) {
            println("<missing>");
            return;
        }
        println("function=" + f.getName(true));
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
        dump(d, 0x004064b0L, "CWorld::EnterTemporaryZone");
        dump(d, 0x00406710L, "CWorld::ExitTemporaryZone");
        dump(d, 0x00405d20L, "CWorld::SwitchMainZone");
        dump(d, 0x004059b0L, "CWorld::FindZone");
        d.dispose();
    }
}
