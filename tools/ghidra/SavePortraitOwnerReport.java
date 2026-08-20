// Resolves the native UI-resource owner used by Load Game's portrait icons.
// Read-only and hash-gated to the supported GOG executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class SavePortraitOwnerReport extends GhidraScript {
    private static final String SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address at(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void dump(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(at(value));
        println("\n===== " + role + " address=" + at(value) + " =====");
        if (function == null) {
            println("missing=true");
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 240,
            monitor);
        println("function=" + function.getName(true));
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "decompile_error=" + result.getErrorMessage());
    }

    @Override
    protected void run() throws Exception {
        if (!SHA256.equalsIgnoreCase(currentProgram.getExecutableSHA256())) {
            throw new Exception("Unexpected executable image");
        }
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] targets = {
            0x0048c8d0L, // Save-entry portrait widget construction/binding
            0x004806f0L, // Load Game page scene/UI initialization
            0x004820a0L, // Load Game page save-data refresh
            0x0055d9c0L, // Shared UI icon group constructor using CycleIcon bind
            0x00559280L, // Live UI owner/provider getter used by the page
            0x0055be70L  // CycleIcon bind
        };
        String[] roles = {
            "save entry portrait construction", "Load Game initialization",
            "Load Game data refresh", "shared CycleIcon owner construction",
            "UI owner provider getter", "CycleIcon bind"
        };
        for (int index = 0; index < targets.length; ++index) {
            dump(decompiler, targets[index], roles[index]);
        }
        decompiler.dispose();
    }
}
