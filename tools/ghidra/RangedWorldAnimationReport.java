// Reports the exact-build model-animation update path used by ranged characters.
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

public class RangedWorldAnimationReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        if (function == null) {
            function = currentProgram.getFunctionManager()
                .getFunctionContaining(address(value));
        }
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
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
        if (function == null) {
            return;
        }
        println("");
        println("CALLERS role=" + role + " function=" +
            function.getEntryPoint());
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
            if (caller != null) {
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
        println("SudekiMP ranged world-animation report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();
        long[] targets = {
            0x005884c0L, // ranged model update owner
            0x004e1930L, // base game-model animation update
            0x004e3780L, // animation clock/state advance
            0x004dfa40L, // pre-update helper
            0x004e36e0L, // animation channel state helper
            0x004e2810L, // animation blend/update helper
            0x004e2bd0L, // animation completion helper
            0x004e0b50L, // animation transition helper
            0x004e05f0L, // animation playback helper
            0x00588630L, // ranged animation predicate
            0x00511b30L, // model attachment switch
            0x00511f70L, // post-attachment model refresh
            0x00511960L, // attachment animation-state save
            0x00511a50L, // attachment animation-state restore
            0x005e84f0L, // attached-model animation selector helper
            0x005e8530L, // attached-model animation rate helper
            0x005e8580L, // attached-model animation time helper
            0x005e85d0L, // attached-model animation channel helper
            0x004e42f0L, // current high-level animation resource lookup
            0x004e1110L, // high-level animation availability lookup
            0x004e1560L, // current high-level animation duration lookup
            0x004e01c0L, // first-person animation-bank transition helper
            0x004e5eb0L  // first-person animation-bank preparation helper
        };
        String[] roles = {
            "ranged model update owner",
            "base game-model animation update",
            "animation clock/state advance",
            "pre-update helper",
            "animation channel state helper",
            "animation blend/update helper",
            "animation completion helper",
            "animation transition helper",
            "animation playback helper",
            "ranged animation predicate",
            "model attachment switch",
            "post-attachment model refresh",
            "attachment animation-state save",
            "attachment animation-state restore",
            "attached-model animation selector helper",
            "attached-model animation rate helper",
            "attached-model animation time helper",
            "attached-model animation channel helper",
            "current high-level animation resource lookup",
            "high-level animation availability lookup",
            "current high-level animation duration lookup",
            "first-person animation-bank transition helper",
            "first-person animation-bank preparation helper"
        };
        for (int index = 0; index < targets.length; ++index) {
            decompile(decompiler, targets[index], roles[index], seen);
            callers(decompiler, targets[index], roles[index], seen);
        }
        decompiler.dispose();
    }
}
