// Traces the exact-build ranged-facing and movement paths needed by the
// split-screen Player 2 camera prototype. Read-only.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class CameraAwareFacingReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(
            DecompInterface decompiler,
            long value,
            String role,
            boolean callers) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        println("");
        println("===== " + role + " target=" + address(value) + " function=" +
            (function == null ? "<none>" : function.getEntryPoint() + " " +
                function.getName(true)) + " =====");
        if (function == null) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() : "Decompiler failed");
        if (!callers) {
            return;
        }
        println("CALLERS");
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) {
                continue;
            }
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP camera-aware facing report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        report(decompiler, 0x004dc530L,
            "first-person movement/facing submission", true);
        report(decompiler, 0x00588870L,
            "ranged first-person controller update", true);
        report(decompiler, 0x004c3650L,
            "movement controller first-person speed update", true);
        report(decompiler, 0x004dada0L,
            "direct arbiter facing helper", true);
        report(decompiler, 0x005113f0L,
            "entity forward-vector getter", true);
        report(decompiler, 0x005114d0L,
            "entity orientation commit", true);
        report(decompiler, 0x004db800L,
            "arbiter movement permission", true);
        report(decompiler, 0x004dbb20L,
            "third-person movement state test", true);
        report(decompiler, 0x004b6e50L,
            "character face-direction helper", true);
        report(decompiler, 0x004f4250L,
            "AI movement direction/mode helper", true);
        report(decompiler, 0x004f4bb0L,
            "AI character controller update", false);
        report(decompiler, 0x004f6880L,
            "AiSetTargetDirection entity overload", true);
        report(decompiler, 0x004f6820L,
            "AiSetTargetDirection angle overload", true);
        report(decompiler, 0x004f77a0L,
            "AiQueueFace angle overload", true);
        report(decompiler, 0x00408960L,
            "CCharacterArbiter IsTurning", true);
        decompiler.dispose();
    }
}
