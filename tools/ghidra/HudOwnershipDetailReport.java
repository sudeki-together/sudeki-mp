// Focused report for UIPortraitGroup/UIPortraitGizmo ownership and update paths.
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

public class HudOwnershipDetailReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null || !seen.add(function.getEntryPoint())) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("");
        println("===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void callers(
            DecompInterface decompiler,
            Function function,
            String role,
            Set<Address> seen) {
        if (function == null) return;
        println("CALLERS role=" + role + " function=" + function.getEntryPoint());
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            if (!reference.getReferenceType().isCall()) continue;
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  callsite=" + reference.getFromAddress() + " caller=" +
                (caller == null ? "<none>" : caller.getEntryPoint() + " " +
                    caller.getName(true)));
            decompile(decompiler, caller, "caller of " + role, seen);
        }
    }

    private void dataReferences(
            DecompInterface decompiler,
            long value,
            String role,
            Set<Address> seen) {
        Address target = address(value);
        println("");
        println("DATA REFERENCES role=" + role + " address=" + target);
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("  reference=" + reference.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            decompile(decompiler, owner, "reference to " + role, seen);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP focused HUD ownership report");
        println("SHA256=" + actualSha256);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] functions = {
            0x00581370L, 0x00581470L, 0x005814e0L,
            0x004aa9e0L, 0x004aaa40L, 0x004aafc0L,
            0x004a95e0L, 0x004aa010L, 0x004ab260L,
            0x004ab8c0L, 0x004ab4c0L
        };
        String[] roles = {
            "UIPortraitGroup vfunc 0", "UIPortraitGroup vfunc 1",
            "UIPortraitGroup vfunc 6", "UIPortraitGizmo vfunc 0",
            "UIPortraitGizmo vfunc 1", "UIPortraitGizmo secondary vfunc 0",
            "UIPortraitGizmo secondary vfunc 1",
            "UIPortraitGizmo secondary vfunc 4",
            "nearby UIPortraitGizmo method", "nearby UIPortraitGizmo method",
            "nearby UIPortraitGizmo method"
        };
        for (int index = 0; index < functions.length; ++index) {
            Function function = currentProgram.getFunctionManager()
                .getFunctionAt(address(functions[index]));
            decompile(decompiler, function, roles[index], seen);
            callers(decompiler, function, roles[index], seen);
        }
        dataReferences(decompiler, 0x006d9004L,
            "UIPortraitGroup vftable", seen);
        dataReferences(decompiler, 0x006cb590L,
            "UIPortraitGizmo primary vftable", seen);
        dataReferences(decompiler, 0x006cb59cL,
            "UIPortraitGizmo secondary vftable", seen);
        dataReferences(decompiler, 0x00808d94L,
            "active CGroupPlayers global", seen);
        decompiler.dispose();
    }
}
