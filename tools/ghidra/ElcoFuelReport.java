// Exact-build, read-only report for Elco's jetpack fuel component and native
// setter boundary.
// @category SudekiMP

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;

public class ElcoFuelReport extends GhidraScript {
    private static final String EXPECTED_SHA256 =
        "8ceb1d3cf667ad906f13252cb5bdf762eb018ebbecb8bffeb92f3b27b0dfbb94";

    private Address address(long value) {
        return currentProgram.getAddressFactory().getDefaultAddressSpace()
            .getAddress(value);
    }

    private void report(DecompInterface decompiler, long value, String label) {
        Address entry = address(value);
        Function function = currentProgram.getFunctionManager()
            .getFunctionAt(entry);
        println("");
        println("===== " + label + " " + entry + " =====");
        if (function == null) {
            println("no function");
            return;
        }
        ReferenceIterator references = currentProgram.getReferenceManager()
            .getReferencesTo(entry);
        while (references.hasNext()) {
            Reference reference = references.next();
            Function caller = currentProgram.getFunctionManager()
                .getFunctionContaining(reference.getFromAddress());
            println("xref=" + reference.getFromAddress() + " type=" +
                reference.getReferenceType() + " caller=" +
                (caller == null ? "<none>" :
                    caller.getEntryPoint() + " " + caller.getName(true)));
        }
        DecompileResults result = decompiler.decompileFunction(
            function, 120, monitor);
        println(result.decompileCompleted() ?
            result.getDecompiledFunction().getC() :
            "Decompiler failed: " + result.getErrorMessage());
    }

    @Override
    protected void run() throws Exception {
        String hash = currentProgram.getExecutableSHA256();
        if (!EXPECTED_SHA256.equalsIgnoreCase(hash)) {
            throw new Exception("Unexpected executable SHA256: " + hash);
        }
        println("SudekiMP Elco jetpack fuel report");
        println("SHA256=" + hash);
        println("ImageBase=" + currentProgram.getImageBase());

        long[] addresses = {
            0x004cdf30L,
            0x004cdf80L,
            0x004cdfe0L,
            0x004cdff0L,
            0x004ce0c0L,
            0x004ce110L,
            0x004ce160L,
            0x0059c8c0L
        };
        String[] labels = {
            "CElcoAbility SetFuel",
            "CElcoAbility SetMaxFuel",
            "CElcoAbility GetFuel",
            "CElcoAbility SetFillupRate",
            "CElcoAbility SetEmptyRate",
            "CElcoAbility ResetFuel",
            "CElcoAbility fuel-state refresh",
            "Fuel point proximity/update consumer"
        };

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        for (int index = 0; index < addresses.length; ++index) {
            report(decompiler, addresses[index], labels[index]);
        }
        decompiler.dispose();
    }
}
