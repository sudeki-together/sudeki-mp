// Reports the exact-build Tal animation retirement and renderer channel path.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TalAnimationRetirementReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private Function functionAtOrContaining(long value) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        return function != null ? function : currentProgram.getFunctionManager()
            .getFunctionContaining(address(value));
    }

    private void decompile(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = functionAtOrContaining(value);
        if (function == null || !seen.add(function.getEntryPoint())) return;
        println("");
        println("===== " + role + " target=" + address(value) +
            " function=" + function.getEntryPoint() + " " +
            function.getName(true) + " =====");
        DecompileResults result = decompiler.decompileFunction(
            function, 90, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    private void callers(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        if (function == null) return;
        println("");
        println("REFERENCES role=" + role + " function=" +
            function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  type=" + reference.getReferenceType() +
                " from=" + reference.getFromAddress() + " owner=" +
                (caller == null ? "<data-or-unknown>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (reference.getReferenceType().isCall() && caller != null) {
                decompile(decompiler, caller.getEntryPoint().getOffset(),
                    "caller of " + role, seen);
            }
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP Tal animation-retirement report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] targets = {
            0x00623000L, // renderer selector setter
            0x00622f80L, // selector-change invalidation callback
            0x00622820L, // primary-submodel selector callback
            0x006230b0L, // renderer selector getter
            0x006230d0L, // renderer rate setter
            0x00623180L, // renderer time setter
            0x00623240L, // renderer state setter
            0x006234c0L, // renderer blend setter
            0x005e84f0L, // all-submodel selector helper
            0x005e8530L, // all-submodel rate helper
            0x005e8580L, // all-submodel time helper
            0x005e85d0L, // all-submodel state helper
            0x004df940L, // high-level animation transition admission
            0x004e4f50L, // high-level animation playback
            0x004e2110L, // high-level channel reset/rebuild
            0x004e1930L, // game-model animation update
            0x004e5390L, // model animation transition notification
            0x004e36e0L, // channel completion notification
            0x004e3640L, // channel state progression
            0x004d20a0L, // melee presentation state commit
            0x004d14d0L  // combo acceptance notification
        };
        String[] roles = {
            "renderer selector setter",
            "selector-change invalidation callback",
            "primary-submodel selector callback",
            "renderer selector getter",
            "renderer rate setter",
            "renderer time setter",
            "renderer state setter",
            "renderer blend setter",
            "all-submodel selector helper",
            "all-submodel rate helper",
            "all-submodel time helper",
            "all-submodel state helper",
            "high-level animation transition admission",
            "high-level animation playback",
            "high-level channel reset/rebuild",
            "game-model animation update",
            "model animation transition notification",
            "channel completion notification",
            "channel state progression",
            "melee presentation state commit",
            "combo acceptance notification"
        };
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index], seen);
            callers(decompiler, targets[index], roles[index], seen);
        }
        decompiler.dispose();
    }
}
