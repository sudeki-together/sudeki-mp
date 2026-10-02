// PlayEntryFullReport — full decompile of FUN_004e4f50 (the ANIMID play entry,
// which crashes when called from the client replica presentation hook) plus its
// resolver FUN_004e42f0, to find what context the play entry requires that the
// replica call path does not provide.
//
// Read-only: refuses any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class PlayEntryFullReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private static final long[] TARGETS = {
        0x004E4F50L,  /* play */
        0x004E42F0L,  /* resolver */
        0x004E1680L   /* state-array allocator */
    };

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void printDecompile(DecompInterface decompiler, long entry, String tag) {
        Function fn = getFunctionAt(address(entry));
        if (fn == null) {
            println(tag + "_ABSENT 0x" + Long.toHexString(entry));
            return;
        }
        println("");
        println("=== " + tag + " " + fn.getName() + " @0x" +
            Long.toHexString(fn.getEntryPoint().getOffset()) + " ===");
        try {
            DecompileResults results = decompiler.decompileFunction(fn, 240, monitor);
            if (results == null || !results.decompileCompleted()) {
                println("decompile_failed " + fn.getName());
                return;
            }
            println("SIGNATURE " + results.getDecompiledFunction().getSignature());
            String[] lines = results.getDecompiledFunction().getC().split("\n");
            for (int index = 0; index < lines.length; ++index) {
                println("  " + lines[index]);
            }
        } catch (Exception error) {
            println("decompile_failed " + fn.getName() + " " + error);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP play-entry full decompile");
        println("SHA256=" + actualSha256);

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);

        for (long target : TARGETS) {
            printDecompile(decompiler, target, "FUNC");
        }

        decompiler.dispose();
        println("");
        println("REPORT_COMPLETE PlayEntryFullReport");
    }
}
