// Trace Sudeki's queued text-submission helper to determine the
// font/layout/alignment argument semantics used by the roster page.
// Read-only and hash-gated to the supported GOG executable.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

import java.util.HashSet;
import java.util.Set;

public class TextSubmitReport extends GhidraScript {
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

    // Show the disassembly around callers to reveal the alignment argument
    // values pushed onto the stack by native call sites.
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
            // print a few instructions before the call to show pushed args
            Address a = ref.getFromAddress();
            for (int i = 0; i < 12 && a != null && !monitor.isCancelled(); ++i) {
                a = a.previous();
                if (a == null) break;
                Instruction ins = currentProgram.getListing()
                    .getInstructionAt(a);
                if (ins == null) break;
                println("      " + a + "  " + ins.toString());
            }
            if (++count >= 60) {
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
        println("SudekiMP queued text-submission report");
        println("SHA256=" + actual);
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        Set<Address> seen = new HashSet<Address>();

        decompile(decompiler, 0x00409930L, "queued text submit (FUN_00409930)", seen);
        decompile(decompiler, 0x004a2140L, "title text/layout submit (FUN_004A2140)", seen);
        callers(0x00409930L, "queued text submit");
        callers(0x004a2140L, "title text/layout submit");

        decompiler.dispose();
    }
}
