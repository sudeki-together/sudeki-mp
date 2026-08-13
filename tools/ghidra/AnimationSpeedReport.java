// Reports the native, per-model animation-speed controls used for Phase 5.
// The script is read-only and refuses to inspect an unexpected executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class AnimationSpeedReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace().getAddress(value);
    }

    private void reportReferences(long value, String role) {
        ReferenceIterator references = currentProgram.getReferenceManager().getReferencesTo(address(value));
        int count = 0;
        println("\n===== " + role + " references =====");
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager().getFunctionContaining(
                reference.getFromAddress());
            println("  " + reference.getFromAddress() + "  " + reference.getReferenceType()
                + "  caller=" + (caller == null ? "<data/non-function>" : caller.getName(true)));
            count++;
        }
        println("  count=" + count);
    }

    private void decompile(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address(value));
        if (function == null) {
            println(role + ": no function at " + address(value));
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
        println("\n===== " + role + " " + function.getEntryPoint() + " "
            + function.getName(true) + " =====");
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

        println("SudekiMP animation-speed static report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        reportReferences(0x004e0460L, "SetAnimationSpeedMultiplier");
        reportReferences(0x004e04b0L, "ResetAnimationSpeedMultiplier");
        reportReferences(0x004e04f0L, "GetAnimationSpeedMultiplier");

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler, 0x004e0460L, "SetAnimationSpeedMultiplier");
        decompile(decompiler, 0x004e04b0L, "ResetAnimationSpeedMultiplier");
        decompile(decompiler, 0x004e04f0L, "GetAnimationSpeedMultiplier");
        decompile(decompiler, 0x005c4b10L, "Script object-method opcode");
        decompile(decompiler, 0x005c4a90L, "Nearby script-object helper");
        decompile(decompiler, 0x005c4c90L, "Nearby script-object helper");
        decompiler.dispose();
    }
}
