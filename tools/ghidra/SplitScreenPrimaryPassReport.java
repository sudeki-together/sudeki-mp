// Reports the exact-build primary render-pass preparation and draw helpers.
// Read-only: refuses to inspect any executable other than the supported GOG build.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class SplitScreenPrimaryPassReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void decompile(DecompInterface decompiler, long value, String role) {
        Function function = currentProgram.getFunctionManager().getFunctionAt(
            address(value));
        println("");
        println("===== " + role + " address=" + address(value) + " function=" +
            (function == null ? "<none>" : function.getName(true)) + " =====");
        if (function == null) {
            return;
        }
        DecompileResults result = decompiler.decompileFunction(function, 90, monitor);
        if (!result.decompileCompleted()) {
            println("Decompiler failed: " + result.getErrorMessage());
            return;
        }
        println(result.getDecompiledFunction().getC());
        println("CALLERS:");
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

    private void instructions(long start, long end, String role) {
        println("");
        println("===== INSTRUCTIONS " + role + " " + address(start) + ".." +
            address(end) + " =====");
        InstructionIterator iterator = currentProgram.getListing().getInstructions(
            new AddressSet(address(start), address(end)), true);
        while (iterator.hasNext() && !monitor.isCancelled()) {
            Instruction instruction = iterator.next();
            println(instruction.getAddress() + "  " + instruction);
        }
    }

    @Override
    protected void run() throws Exception {
        String actualSha256 = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(actualSha256)) {
            throw new Exception("Unexpected executable SHA256: " + actualSha256);
        }
        println("SudekiMP split-screen primary-pass report");
        println("SHA256=" + actualSha256);
        println("ImageBase=" + currentProgram.getImageBase());

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        long[] functions = {
            0x005d4750L,
            0x005d4820L,
            0x005d48c0L,
            0x005e2720L,
            0x005d4fe0L,
            0x005d4e70L,
            0x005d5130L,
            0x00626f30L,
            0x005d5600L,
            0x005d51f0L,
            0x005d5260L,
            0x005d5300L,
            0x0062faa0L,
            0x005dce30L,
            0x004a2900L,
            0x00503a40L
        };
        String[] roles = {
            "primary visible render helper",
            "middle render helper",
            "first render helper",
            "render state selector",
            "primary pass mode",
            "primary pass flag submission",
            "primary pass draw list",
            "primary pass post draw",
            "render finalizer",
            "first helper stage A",
            "middle helper stage",
            "first helper stage B",
            "first helper object submission",
            "viewport/projection update",
            "SetScreenOffset",
            "SetScreenScale"
        };
        for (int index = 0; index < functions.length; ++index) {
            decompile(decompiler, functions[index], roles[index]);
        }
        instructions(0x0068d430L, 0x0068d490L,
            "main primary render sequence");
        decompiler.dispose();
    }
}
