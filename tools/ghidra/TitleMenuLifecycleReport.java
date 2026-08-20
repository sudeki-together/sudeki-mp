// Reports the native title controller and its resident row-object lifecycle.
// Read-only and hash-gated to the supported GOG executable.
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

public class TitleMenuLifecycleReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(DecompInterface decompiler, long value, String role,
            Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        println("\n===== " + role + " address=" + address(value) + " =====");
        if (function == null) {
            println("function_missing=true");
            return;
        }
        if (!seen.add(function.getEntryPoint())) {
            println("already_reported=" + function.getEntryPoint());
            return;
        }
        println("function=" + function.getEntryPoint() + " " +
            function.getName(true));
        DecompileResults result = decompiler.decompileFunction(function, 90,
            monitor);
        if (result.decompileCompleted()) {
            println(result.getDecompiledFunction().getC());
        } else {
            println("decompile_error=" + result.getErrorMessage());
        }
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(function.getEntryPoint());
        int calls = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("caller=" + ref.getFromAddress() + " owner=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            ++calls;
        }
        println("callers=" + calls);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP title-menu lifecycle report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] targets = {
            0x0049e730L, // PC front-end owner construction/allocation
            0x004a2940L, // PC front-end controller construction/init
            0x004a3020L, // controller page/state rebuild
            0x004a3ff0L, // controller page transition setup
            0x004a4490L, // controller resident-page cleanup/reset
            0x004a4590L, // title activation helper
            0x004a16f0L, // resident title-row selection/highlight refresh
            0x004a1950L, // localized label presentation replacement
            0x004a3760L, // PC front-end render/submit pass
            0x004a1900L, // native title row reset
            0x004a27e0L, // native front-end setup/reset
            0x00520260L, // UI object state request
            0x00520370L, // UI object state request variant
            0x00520400L, // UI object update/flush
            0x00520550L, // UI object state completion
            0x0055b150L, // UI resource/page helper
            0x0055b640L, // UI object construction owner
            0x0055b740L, // UI object active-state helper
            0x0055c070L, // UI object update owner
            0x0055c190L, // UI object release/reset
            0x0055ef70L, // UI resource construction caller
            0x00559190L, // UI manager helper
            0x00559280L  // UI manager object-list getter
        };
        String[] roles = {
            "PC front-end owner construction/allocation",
            "PC front-end controller init",
            "front-end page rebuild",
            "front-end page transition setup",
            "resident page cleanup/reset",
            "title activation helper",
            "resident title-row selection/highlight refresh",
            "localized label presentation replacement",
            "PC front-end render/submit pass",
            "native title row reset",
            "native front-end setup/reset",
            "UI state request",
            "UI state request variant",
            "UI update/flush",
            "UI state completion",
            "UI resource/page helper",
            "UI object construction owner",
            "UI active-state helper",
            "UI object update owner",
            "UI object release/reset",
            "UI resource construction caller",
            "UI manager helper",
            "UI manager object-list getter"
        };
        for (int index = 0; index < targets.length; ++index) {
            report(decompiler, targets[index], roles[index], seen);
        }
        decompiler.dispose();
    }
}
