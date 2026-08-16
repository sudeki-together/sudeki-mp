// Reports Sudeki's command-line parser used by the native test-level path.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class InputArgsReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, long target, String role) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(target));
        println("");
        println("===== " + role + " " + address(target) + " " +
            (function == null ? "<none>" : function.getName(true)) + " =====");
        if (function == null) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
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
        println("SudekiMP command-line parser report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler, 0x00422300L, "CInputArgs value lookup");
        decompile(decompiler, 0x00422260L, "CInputArgs startup parse");
        decompile(decompiler, 0x00476cd0L, "misidentified adjacent singleton init");
        decompile(decompiler, 0x0052aac0L, "CInputArgs argument-list cleanup");
        decompiler.dispose();
    }
}
