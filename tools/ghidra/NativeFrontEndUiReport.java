// Trace the native front-end menu/UI construction path.
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

public class NativeFrontEndUiReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, long value,
            String role, Set<Address> seen) {
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(address(value));
        if (function == null || !seen.add(function.getEntryPoint())) {
            println("MISSING_OR_ALREADY_REPORTED role=" + role +
                " address=" + address(value));
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90,
            monitor);
        println("\n===== " + role + " function=" + function.getEntryPoint() +
            " " + function.getName(true) + " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void callers(long value, String role) {
        Address target = address(value);
        println("\nCALLERS role=" + role + " address=" + target);
        ReferenceIterator refs = currentProgram.getReferenceManager()
            .getReferencesTo(target);
        int count = 0;
        while (refs.hasNext() && !monitor.isCancelled()) {
            Reference ref = refs.next();
            if (!ref.getReferenceType().isCall()) {
                continue;
            }
            Function owner = currentProgram.getFunctionManager()
                .getFunctionContaining(ref.getFromAddress());
            println("  callsite=" + ref.getFromAddress() + " caller=" +
                (owner == null ? "<none>" : owner.getEntryPoint() + " " +
                    owner.getName(true)));
            if (++count >= 80) {
                println("  callers_truncated=true");
                break;
            }
        }
        println("  callers=" + count);
    }

    @Override
    protected void run() throws Exception {
        String actual = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actual)) {
            throw new Exception("Unexpected executable SHA256: " + actual);
        }
        println("SudekiMP native front-end UI report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        long[] targets = {
            0x004a1950L, // native menu builder
            0x004a0f40L, // front-end state/update
            0x004a0360L, // action dispatcher
            0x004a16f0L, // native selected-item refresh
            0x004a2770L, // native menu transition/cleanup
            0x005d92e0L, // UI resource loader used by native UI constructors
            0x0055c0e0L, // native menu/item UI registration path
            0x005b9fc0L, // localized-label lookup/registration
            0x004049c0L, // action binding
            0x0040a820L, // CUIScene queued UI flush
            0x0049bba0L, // QuickMenu UI submission example
            0x004a2ca0L, // PC front-end controller update vslot
            0x004a3760L, // PC front-end controller render/submit vslot
            0x004a3510L, // PC front-end controller vslot
            0x004a3960L, // PC front-end controller vslot
            0x004a3940L, // PC front-end controller vslot
            0x004a2900L, // PC front-end controller shared vslot
            0x004a3250L, // PC front-end controller vslot
            0x004a2c90L, // PC front-end controller vslot
            0x004a4510L  // PC front-end controller vslot
            ,0x004a1830L // append/copy native title menu entry
            ,0x004a2140L // native title menu text/layout submission
            ,0x004a0cb0L // native title controller update helper
        };
        String[] roles = {
            "native menu builder", "front-end state/update", "action dispatcher",
            "native selected-item refresh", "native menu transition/cleanup",
            "UI resource loader", "native menu/item registration",
            "localized-label registration", "action binding",
            "CUIScene UI flush", "QuickMenu native UI submit",
            "PC front-end update vslot", "PC front-end render vslot",
            "PC front-end vslot 0x10", "PC front-end vslot 0x14",
            "PC front-end vslot 0x18", "PC front-end shared vslot",
            "PC front-end vslot 0x2c", "PC front-end vslot 0x38",
            "PC front-end vslot 0x48",
            "native title entry append", "native title text/layout submit",
            "native title update helper"
        };
        for (int i = 0; i < targets.length; ++i) {
            decompile(decompiler, targets[i], roles[i], seen);
            callers(targets[i], roles[i]);
        }
        decompiler.dispose();
    }
}
