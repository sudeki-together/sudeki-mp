// Reports the character-spawn command-line flags used by Sudeki test levels.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class TestArenaPlayerArgsReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(0x00406ae0L));
        if (function == null) {
            throw new Exception("Missing test-level player initialization function");
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        if (!result.decompileCompleted()) {
            throw new Exception(result.getErrorMessage());
        }
        String source = result.getDecompiledFunction().getC();
        int first = source.indexOf("\"-Tal\"");
        int last = source.indexOf("\"-Cluster\"");
        println("SudekiMP test-arena player-argument report");
        println("SHA256=" + actualSha256);
        println("function=" + function.getEntryPoint());
        if (first < 0 || last < first) {
            println(source);
        } else {
            first = Math.max(0, first - 1200);
            last = Math.min(source.length(), last + 300);
            println(source.substring(first, last));
        }
        decompiler.dispose();
    }
}
