// Reports the exact-build UIBlackSmithStart entry, every direct caller, and
// the surrounding global UI-mode request helpers. Read-only: no project or
// executable mutation.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class BlacksmithStartReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(
            DecompInterface decompiler, Function function, String role) {
        if (function == null) {
            println("MISSING role=" + role);
            return;
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println("\n===== " + role + " function=" +
            function.getEntryPoint() + " " + function.getName(true) +
            " =====");
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
    }

    private void callers(
            DecompInterface decompiler, long target, String role) {
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(address(target));
        int count = 0;

        println("\nREFERENCES role=" + role + " target=" + address(target));
        while (references.hasNext() && !monitor.isCancelled()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("REFERENCE from=" + reference.getFromAddress() +
                " type=" + reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
            if (reference.getReferenceType().isCall()) {
                decompile(decompiler, caller, role + " caller");
            }
            ++count;
        }
        println("REFERENCE_COUNT role=" + role + " count=" + count);
    }

    @Override
    protected void run() throws Exception {
        String sha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(sha256)) {
            throw new Exception("Unexpected executable SHA256: " + sha256);
        }
        println("SudekiMP blacksmith-start report");
        println("SHA256=" + sha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                address(0x00492c40L)),
            "UIBlackSmithStart");
        decompile(decompiler,
            currentProgram.getFunctionManager().getFunctionAt(
                address(0x00492c60L)),
            "UIBlackSmithActive");
        callers(decompiler, 0x00492c40L, "UIBlackSmithStart");
        callers(decompiler, 0x00492c60L, "UIBlackSmithActive");
        callers(decompiler, 0x00403590L, "UI mode request helper");
        decompiler.dispose();
    }
}
